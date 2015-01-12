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

/************************************************************/

static int do_trig(struct ultrasonic_dev *dev)
{
    /* keeping 10 us at HIGH level */
    bcm2835_gpio_write(dev->pin_trig, HIGH);
    if (evtimer_add(dev->ev_over_trig, &dev->tv_trig) < 0)
        return -ENOSPC;
    return 0;
}

static void echo_signal(int fd, short what, void *arg)
{
    struct ultrasonic_dev *dev = arg;
    struct timespec tp, elapse;
    unsigned long microsec;
    double distance;
    int level;

    dev->nr_echo++;
    clock_gettime(CLOCK_MONOTONIC, &tp);
    level = (int)bcm2835_gpio_lev(dev->pin_echo);
    /*
    fprintf(stderr, "nr_trig = %d, nr_echo = %d, level = %d, sec = %d, nsec = %d\n",
                        dev->nr_trig, dev->nr_echo, level, tp.tv_sec, tp.tv_nsec);
    */
    if (level) {
        dev->echo_tp = tp;
    } else {
        timespec_sub(&tp, &dev->echo_tp, &elapse);
        microsec = elapse.tv_sec * 1000000 + elapse.tv_nsec / 1000;
        distance = US2VELOCITY(microsec);
        if (dev->cb) {
            int err;

            err = dev->cb(dev, distance, dev->opaque);
            if (err < 0 && evtimer_pending(dev->ev_timer, NULL))
                evtimer_del(dev->ev_timer);
        }

        if (dev->flags & UF_IMMEDIATE) {
            /* TODO */
            if (dev->count == -1 || dev->counted++ < dev->count)
                do_trig(dev);
        }
    }
}

static void trig_done(int fd, short what, void *arg)
{
    struct ultrasonic_dev *dev = arg;
    dev->nr_trig++;
    bcm2835_gpio_write(dev->pin_trig, LOW);
    evtimer_del(dev->ev_over_trig);
}

static void timer_scope(int fd, short what, void *arg);

struct ultrasonic_dev *ultrasonic_new(int pin_trig,
                                int pin_echo, int trig_time)
{
    struct ultrasonic_dev *dev;

    dev = malloc(sizeof(*dev));
    if (dev) {
        memset(dev, 0, sizeof(*dev));
        dev->pin_trig = pin_trig;
        dev->pin_echo = pin_echo;
        dev->trig_time = trig_time;

        dev->tv_trig.tv_sec = 0;
        dev->tv_trig.tv_usec = trig_time;

        /* only new, no add */
        dev->ev_over_trig = evtimer_new(evbase, trig_done, dev);
        if (dev->ev_over_trig == NULL) {
            ultrasonic_del(dev);
            return NULL;
        }
        dev->ev_timer = event_new(evbase, -1, EV_PERSIST, timer_scope, dev);
        if (dev->ev_timer == NULL) {
            ultrasonic_del(dev);
            return NULL;
        }

        if (bcm2835_gpio_signal(dev->pin_echo, EDGE_both,
                        echo_signal, dev, &dev->ev_echo) < 0) {
            ultrasonic_del(dev);
            return NULL;
        }

        bcm2835_gpio_fsel(dev->pin_trig, BCM2835_GPIO_FSEL_OUTP);
        bcm2835_gpio_write(dev->pin_trig, LOW);
    }
    return dev;
}

void ultrasonic_del(struct ultrasonic_dev *dev)
{
    if (dev) {
        if (dev->ev_timer)
            eventfd_del(dev->ev_timer);
        if (dev->ev_echo)
            eventfd_del(dev->ev_echo);
        if (dev->ev_over_trig)
            eventfd_del(dev->ev_over_trig);
        free(dev);
    }
}

int ultrasonic(struct ultrasonic_dev *dev, __cb_ultrasonic cb, void *opaque)
{
    /* in using ? */
    if (evtimer_pending(dev->ev_over_trig, NULL))
        return -EBUSY;

    dev->cb = cb;
    dev->opaque = opaque;
    return do_trig(dev);
}

static void timer_scope(int fd, short what, void *arg)
{
    struct ultrasonic_dev *dev = arg;

    /* check the count */
    if (dev->count != -1 && dev->counted++ >= dev->count) {
        event_del(dev->ev_timer);
        return;
    }

    if (do_trig(dev) < 0) {
        /* TODO */
    }
}

/*
 * interval:
 *      < 0  delete
 *      = 0  immediately after occured
 *      > 0  interval
 */
int ultrasonic_scope(struct ultrasonic_dev *dev, int count,
                int interval, __cb_ultrasonic cb, void *opaque)
{
    struct timeval tv;

    dev->flags = 0;
    dev->counted = 0;
    dev->count = count;

    if (interval < 0) {
        if (evtimer_pending(dev->ev_timer, NULL)) {
            evtimer_del(dev->ev_timer);
            return 0;
        } else {
            return ENOENT;
        }
    } else if (interval == 0) {
        /* TODO */
        dev->flags |= UF_IMMEDIATE;
        return ultrasonic(dev, cb, opaque);
    }

    dev->cb = cb;
    dev->opaque = opaque;

    tv.tv_sec = interval / 1000;
    tv.tv_usec = (interval % 1000) * 1000;
    if (evtimer_add(dev->ev_timer, &tv) < 0)
        return -ENOSPC;
    return 0;
}

unsigned int ultrasonic_is_busy(struct ultrasonic_dev *dev)
{
    return (unsigned int)(evtimer_pending(dev->ev_timer, NULL)
                    || evtimer_pending(dev->ev_over_trig, NULL));
}

/************************************************************/

static int cb_main(struct ultrasonic_dev *dev, double distance, void *opaque)
{
    int wfd = (intptr_t)opaque;
    char buffer[128];
    size_t len;

    if (wfd != -1) {
        len = snprintf(buffer, sizeof(buffer),
                "ultrasonic: distance = %.3f cm\n", distance);
        write(wfd, buffer, len);
    }

    return 0;
}

static int ultrasonic_main(int fd, int argc, char *argv[])
{
    int count = 1;
    int interval = 2000;
    struct ultrasonic_dev *dev;
    static struct option options[] = {
        { "stop",     no_argument,       NULL, 'e' },
        { "count",    required_argument, NULL, 'n' },
        { "interval", required_argument, NULL, 't' },
        { 0, 0, 0, 0 }
    };
    int c;

    while ((c = getopt_long(argc, argv, "en:t:", options, NULL)) != -1) {
        switch (c) {
        case 'e': interval = -1; break;
        case 'n': count = atoi(optarg); break;
        case 't': interval = atoi(optarg); break;
        default:
            return 1;
        }
    }

    dev = luaenv_getdev(MODNAME);
    if (dev == NULL)
        return 1;

    if (ultrasonic_scope(dev, count, interval,
                cb_main, (void *)(intptr_t)fd) < 0)
        return 1;

    return 0;
}

DEFINE_MODULE(ultrasonic);
