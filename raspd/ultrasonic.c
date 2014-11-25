#include <stdio.h>
#include <stdlib.h>
#include <event2/event.h>

#include <bcm2835.h>

#include "module.h"
#include "event.h"

struct ultrasonic_env {
    int pin_trig;
    int pin_echo;
    int nr_trig;
    int nr_echo;
    int count;
    int counted;
    int interval;
    struct event *ev_timer;
    struct event *ev_over_trig;
    struct timespec lasttime;     /* last time */
    int trigged;
    int wfd;
};

static int cb_echo(int nr, int value, void *opaque)
{
    struct ultrasonic_env *env = arg;
    struct timespec tp;

    clock_gettime(CLOCK_MONOTONIC, &tp);
    if (env->trigged) {
    } else {
        env->lasttime = tp;
        env->trigged = 1;
    }
}

static void cb_over_trig(int fd, short what, void *arg)
{
    struct ultrasonic_env *env = arg;
    bcm2835_gpio_write(env->pin_trig, LOW);
    evtimer_del(env->ev_over_trig);
}

static void cb_timer(int fd, short what, void *arg)
{
    static struct timeval tv = { 0, 10 };
    struct ultrasonic_env *env = arg;

    /* check the count */
    if (env->counted++ >= env->count) {
        eventfd_del(env->ev_timer);
        enentfd_del(env->ev_over_trig);
        free(env);
        return;
    }

    /*
     * do measure
     * keeping 10 us at HIGH level
     */
    bcm2835_gpio_write(env->pin_trig, HIGH);
    evtimer_add(env->ev_over_trig, &tv);
}

static int ultrasonic_main(int wfd, int argc, char *argv[])
{
    int count = 1;
    int interval = 2000;
    static struct option options[] = {
        { "stop",     no_argument,       NULL, 's' },
        { "pin-trig", required_argument, NULL, 'i' },
        { "pin-echo", required_argument, NULL, 'o' },
        { "count",    required_argument, NULL, 'n' },
        { "interval", required_argument, NULL, 't' },
        { 0, 0, 0, 0 }
    };
    int c;

    while ((c = getopt_long(argc, argv, "si:o:n:t:", options, NULL)) != -1) {
        switch (c) {
        case 's':
        case 'i':
        case 'o':
        case 'n':
        case 't':
        }
    }

    if (count >= 1 && interval) {
        struct timeval t;

        t.tv_sec = interval / 1000;
        t.tv_usec = (interval % 1000) * 1000000;

        if (register_timer(EV_PERSIST, &t, cb_timer, NULL, &ev);
    }
}

DEFINE_MODULE(ultrasonic);
