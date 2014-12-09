#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <getopt.h>
#include <time.h>
#include <unistd.h>
#include <event2/event.h>

#include <xmalloc.h>
#include <bcm2835.h>

#include "module.h"
#include "event.h"
#include "gpiolib.h"
#include "luaenv.h"

#include "ultrasonic.h"

#define MODNAME     "ultrasonic"

#define PIN_TRIG    20
#define PIN_ECHO    21

/*
 * in air
 *    0 degree: 331 m/s
 *   15 degree: 340 m/s
 *   25 degree: 346 m/s
 *
 * 340 m/s = (340 * 100) / 1000000 cm/us = 34 / 1000 cm/us
 */
#define VELOCITY_VOICE  ((double)346 / 10000)
#define US2VELOCITY(x)  ((double)x * VELOCITY_VOICE / 2)

#define timespec_sub(tvp, uvp, vvp)                       \
    do {                                                  \
        (vvp)->tv_sec = (tvp)->tv_sec - (uvp)->tv_sec;    \
        (vvp)->tv_nsec = (tvp)->tv_nsec - (uvp)->tv_nsec; \
        if ((vvp)->tv_nsec < 0) {                         \
            (vvp)->tv_sec--;                              \
            (vvp)->tv_nsec += 1000000000;                 \
        }                                                 \
    } while (0)

struct ultrasonic_env {
    int pin_trig;
    int pin_echo;
    int nr_trig;
    int nr_echo;
    int count;
    int counted;
    int interval;   /* millisecond */
    struct event *ev_timer;
    struct event *ev_over_trig;
    struct event *ev_echo;
    struct timespec trig_tp;
    struct timespec echo_tp;     /* last time */
    /*int wfd;*/
    void (*urgent_cb)(double distance/* cm */, void *opaque);
    void *opaque;
};

static int pin_trig = PIN_TRIG;
static int pin_echo = PIN_ECHO;

/* urgent scope, double ? */
static int threshold;

#define MAX_LFUNCNAME   64
static char callback[MAX_LFUNCNAME];

static free_env(struct ultrasonic_env *env)
{
    if (env->ev_timer)
        eventfd_del(env->ev_timer);
    if (env->ev_over_trig)
        eventfd_del(env->ev_over_trig);
    if (env->ev_echo)
        eventfd_del(env->ev_echo);
    free(env);
}

static void cb_echo(int fd, short what, void *arg)
{
    struct ultrasonic_env *env = arg;
    struct timespec tp, elapse;
    unsigned long microsec;
    double distance;
    int value;

    env->nr_echo++;
    clock_gettime(CLOCK_MONOTONIC, &tp);
    value = (int)bcm2835_gpio_lev(env->pin_echo);
    /*
    fprintf(stderr, "nr = %d, value = %d, sec = %d, nsec = %d\n",
                        env->nr_echo, value, tp.tv_sec, tp.tv_nsec);
    */
    if (value) {
        env->echo_tp = tp;
    } else {
        timespec_sub(&tp, &env->echo_tp, &elapse);
        microsec = elapse.tv_sec * 1000000 + elapse.tv_nsec / 1000;
        distance = (double)microsec * VELOCITY_VOICE / 2;
        distance = US2VELOCITY(microsec);

        if (env->urgent_cb)
            env->urgent_cb(distance, env->opaque);
    }
}

static void cb_over_trig(int fd, short what, void *arg)
{
    struct ultrasonic_env *env = arg;
    env->nr_trig++;
    bcm2835_gpio_write(env->pin_trig, LOW);
    evtimer_del(env->ev_over_trig);
}

static void cb_timer(int fd, short what, void *arg)
{
    static struct timeval tv = { 0, 10 };
    struct ultrasonic_env *env = arg;

    /* check the count */
    if (env->counted++ >= env->count) {
        free_env(env);
        return;
    }

    /*
     * do measure
     * keeping 10 us at HIGH level
     */
    bcm2835_gpio_write(env->pin_trig, HIGH);
    clock_gettime(CLOCK_MONOTONIC, &env->trig_tp);
    evtimer_add(env->ev_over_trig, &tv);
}

int ultrasonic_scope(int count, int interval,
            void (*urgent_cb)(double distance/* cm */, void *opaque),
            void *opaque)
{
    struct ultrasonic_env *env;
    struct timeval tv;

    env = xmalloc(sizeof(*env));
    memset(env, 0, sizeof(*env));
    env->pin_trig = pin_trig;
    env->pin_echo = pin_echo;
    env->count = count;
    env->interval = interval;
    env->urgent_cb = urgent_cb;
    env->opaque = opaque;

    /* XXX  not add */
    env->ev_over_trig = evtimer_new(base, cb_over_trig, env);
    if (env->ev_over_trig == NULL) {
        free_env(env);
        return -ENOMEM;
    }

    if (bcm2835_gpio_signal(env->pin_echo, EDGE_both,
                        cb_echo, env, &env->ev_echo) < 0) {
        free_env(env);
        return -EIO;
    }

    bcm2835_gpio_fsel(env->pin_trig, BCM2835_GPIO_FSEL_OUTP);
    bcm2835_gpio_write(env->pin_trig, LOW);
    tv.tv_sec = interval / 1000;
    tv.tv_usec = (interval % 1000) * 1000000;
    if (register_timer(EV_PERSIST, &tv,
                cb_timer, env, &env->ev_timer) < 0) {
        free_env(env);
        return -ENOENT;
    }
    return 0;
}

static void urgent_cb(double distance/* cm */, void *opaque)
{
    int wfd = (int)opaque;
    char buffer[128];
    size_t len;

    len = snprintf(buffer, sizeof(buffer),
            "ultrasonic: distance = %.3f cm\n", distance);
    write(wfd, buffer, len);

    /* call the lua function urgent_cb */
    if (threshold && distance <= (double)threshold && callback[0]) {
        int err;

        if ((err = luaenv_call_va(callback, "id:", wfd, distance)) < 0)
            fprintf(stderr, "luaenv_call_va(%s), err = %d\n", callback, err);
    }
}

static int ultrasonic_main(int wfd, int argc, char *argv[])
{
    int count = 1;
    int interval = 2000;
    struct ultrasonic_env *env;
    static struct option options[] = {
        { "stop",     no_argument,       NULL, 's' },
        { "pin-trig", required_argument, NULL, 'i' },
        { "pin-echo", required_argument, NULL, 'o' },
        { "count",    required_argument, NULL, 'n' },
        { "interval", required_argument, NULL, 't' },
        { 0, 0, 0, 0 }
    };
    int c;
    int err;

    while ((c = getopt_long(argc, argv, "si:o:n:t:", options, NULL)) != -1) {
        switch (c) {
        case 's': break;
        case 'i': pin_trig = atoi(optarg); break;
        case 'o': pin_echo = atoi(optarg); break;
        case 'n': count = atoi(optarg); break;
        case 't': interval = atoi(optarg); break;
        default:
            return 1;
        }
    }

    if (count >= 1 && interval) {
        if (ultrasonic_scope(count, interval, urgent_cb, (void *)wfd) < 0)
            return 1;
    }

    return 0;
}

static int ultrasonic_init(void)
{
    const char *script = NULL;
    const char *cb = NULL;
    int err;

    luaenv_getconf_int(MODNAME, "TRIG", &pin_trig);
    luaenv_getconf_int(MODNAME, "ECHO", &pin_echo);
    luaenv_getconf_int(MODNAME, "threshold", &threshold);

    luaenv_getconf_str(MODNAME, "script", &script);
    if (script) {
        if ((err = luaenv_run_file(script)) < 0)
            fprintf(stderr, "luaenv_run_file(%s), err = %d\n", script, err);
        luaenv_pop(1);
    }

    luaenv_getconf_str(MODNAME, "callback", &cb);
    if (cb) {
        strncpy(callback, cb, MAX_LFUNCNAME);
        luaenv_pop(1);
    }

    return 0;
}

/*
 * DEFINE_MODULE(ultrasonic);
 */
static struct module __module_ultrasonic = {
    .name = "ultrasonic",
    .init = ultrasonic_init,
    .main = ultrasonic_main
};

static __init void __reg_module_ultrasonic(void)
{
    register_module(&__module_ultrasonic);
}
