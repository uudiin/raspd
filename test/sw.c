#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <getopt.h>
#include <time.h>
#include <termios.h>
#include <event2/event.h>

#include <xmalloc.h>
#include <bcm2835.h>
#include "../raspd/event.h"

#define INVALID_PIN(x)      ((x) < 0 || (x) >= NR_BCM2835_GPIO)
#define min(x, y) (((x) < (y)) ? (x) : (y))
#define max(x, y) (((x) > (y)) ? (x) : (y))

#define NR_WING 4

struct esc_env {
    int pin[NR_WING];
    int keep_time;
    int min_keep_time; /* 900 us */
    int max_keep_time; /* 2100 us */
    int interval;
    struct event *timer;
    struct event *ev_done;
};

static void trig_done(int fd, short what, void *arg)
{
    struct esc_env *env = arg;
    int i;
    for (i = 0; i < NR_WING; i++) {
        if (env->pin[i])
            bcm2835_gpio_write(env->pin[i], LOW);
    }
}

static void cb_timer(int fd, short what, void *arg)
{
    struct esc_env *env = arg;
    struct timeval tv;
    int i;

    for (i = 0; i < NR_WING; i++) {
        if (env->pin[i])
            bcm2835_gpio_write(env->pin[i], HIGH);
    }
 
    tv.tv_sec = env->keep_time / 1000000;
    tv.tv_usec = env->keep_time % 1000000;
    evtimer_add(env->ev_done, &tv);
}

static void cb_read(int fd, short what, void *arg)
{
    struct esc_env *env = arg;
    char buffer[512];
    int new = env->keep_time;
    int size;

    size = read(fd, buffer, sizeof(buffer));
    if (size < 1)
        return;

    if (size == 1) {
        switch (buffer[0]) {
        case 'k': new += 20; break; /* up */
        case 'j': new -= 20; break; /* down */
        case 'l': new += 200; break; /* right */
        case 'h': new -= 200; break; /* left */
        }
    } else if (size >= 3 && buffer[0] == 0x1b && buffer[1] == 0x5b) {
        if (size == 3) {
            switch (buffer[2]) {
            case 0x41: new += 1;  break; /* up */
            case 0x42: new -= 1;  break; /* down */
            case 0x43: new += 10; break; /* right */
            case 0x44: new -= 10; break; /* left */
            }
        } else if (size == 4 && buffer[3] == 0x7e) {
            switch (buffer[2]) {
            case 0x31: new = env->min_keep_time; break; /* home */
            case 0x34: new = env->max_keep_time; break; /* end */
            case 0x35: new += 100; break;           /* pageup */
            case 0x36: new -= 100; break;           /* pagedown */
            }
        }
    }

    if (new == env->keep_time)
        return;

    env->keep_time = min(new, env->max_keep_time);
    env->keep_time = max(env->keep_time, env->min_keep_time);
    fprintf(stdout, "keep_time = %d\n", env->keep_time);
}

static struct termios old, new;

static void init_termios(int echo) 
{
	tcgetattr(0, &old); /* grab old terminal i/o settings */
	new = old; /* make new settings same as old settings */
	new.c_lflag &= ~ICANON; /* disable buffered i/o */
	new.c_lflag &= echo ? ECHO : ~ECHO; /* set echo mode */
	tcsetattr(0, TCSANOW, &new); /* use these new terminal i/o settings now */
}

/* Restore old terminal i/o settings */
static void reset_termios(void) 
{
	tcsetattr(0, TCSANOW, &old);
}

/*
 * usage:
 *   sw --hz 50 --min 900 --max 2100 18
 */
int main(int argc, char *argv[])
{
    static struct option options[] = {
        { "hz",  required_argument, NULL, 'f' },
        { "min", required_argument, NULL, 'l' },
        { "max", required_argument, NULL, 'h' },
        { "full", no_argument,      NULL, 'u' },
        { 0, 0, 0, 0 }
    };
    int c;
    int i;
    int interval;  /* millisecond */
    struct esc_env *env;
    struct timeval timeout;
    struct event *ev_stdin;
    int hz = 50; /* 50 Hz, 20 ms */
    int min_keep_time = 900;
    int max_keep_time = 2100;
    int full_speed = 0;
    int err = -EFAULT;

    while ((c = getopt_long(argc, argv, "f:l:h:u", options, NULL)) != -1) {
        switch (c) {
        case 'f': hz = atoi(optarg); break;
        case 'l': min_keep_time = atoi(optarg); break;
        case 'h': max_keep_time = atoi(optarg); break;
        case 'u': full_speed = 1; break;
        default:
            return 1;
        }
    }

    if (optind >= argc)
        return 1;

    init_termios(0);
    if (!bcm2835_init())
        return 1;
    rasp_event_init();

    interval = 1000 / hz;

    do {
        env = xmalloc(sizeof(*env));
        memset(env, 0, sizeof(*env));
        env->min_keep_time = min_keep_time;
        env->max_keep_time = max_keep_time;
        env->interval = interval;
        timeout.tv_sec = interval / 1000;
        timeout.tv_usec = (interval % 1000) * 1000;
        for (i = 0; optind < argc; i++, optind++) {
            env->pin[i] = atoi(argv[optind]);
            if (env->pin[i])
                bcm2835_gpio_fsel(env->pin[i], BCM2835_GPIO_FSEL_OUTP);
        }
        if (full_speed)
            env->keep_time = max_keep_time;
        else
            env->keep_time = min_keep_time;

        err = -ENOMEM;
        env->ev_done = evtimer_new(base, trig_done, env);
        if (env->ev_done == NULL)
            break;

        err = eventfd_add(STDIN_FILENO, EV_READ | EV_PERSIST,
                                NULL, cb_read, env, &ev_stdin);
        if (err < 0) {
            fprintf(stderr, "eventfd_add(STDIN_FILENO), err = %d\n", err);
            return 1;
        }

        err = -EIO;
        if (register_timer(EV_PERSIST, &timeout, cb_timer, env, &env->timer) < 0)
            break;

        event_base_dispatch(base);

        err = 0;
    } while (0);

    if (err != 0) {
        if (env) {
            if (env->ev_done)
                event_free(env->ev_done);
            if (env->timer)
                event_free(env->timer);
            free(env);
        }

        return 1;
    }

    rasp_event_exit();
    bcm2835_close();
    reset_termios();

    return 0;
}
