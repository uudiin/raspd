#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <event2/event.h>

#include <xmalloc.h>
#include <bcm2835.h>

#include "event.h"
#include "motor.h"

static void cb_timer(int fd, short what, void *arg);

#define INIT_PULSE_FOUR(x, pin1, pin2, pin3, pin4)  \
    do {                                            \
        (x)[0] = 1 << (pin1);                       \
        (x)[1] = 1 << (pin2);                       \
        (x)[2] = 1 << (pin3);                       \
        (x)[3] = 1 << (pin4);                       \
    } while (0)

#define INIT_PULSE_DFOUR(x, pin1, pin2, pin3, pin4) \
    do {                                            \
        (x)[0] = 1 << (pin1) | 1 << (pin2);         \
        (x)[1] = 1 << (pin2) | 1 << (pin3);         \
        (x)[2] = 1 << (pin3) | 1 << (pin4);         \
        (x)[3] = 1 << (pin4) | 1 << (pin1);         \
    } while (0)

#define INIT_PULSE_EIGHT(x, pin1, pin2, pin3, pin4) \
    do {                                            \
        (x)[0] = 1 << (pin1);                       \
        (x)[1] = 1 << (pin1) | 1 << (pin2);         \
        (x)[2] = 1 << (pin2);                       \
        (x)[3] = 1 << (pin2) | 1 << (pin3);         \
        (x)[4] = 1 << (pin3);                       \
        (x)[5] = 1 << (pin3) | 1 << (pin4);         \
        (x)[6] = 1 << (pin4);                       \
        (x)[7] = 1 << (pin4) | 1 << (pin1);         \
    } while (0)

#define PIN_MASK(pin1, pin2, pin3, pin4)            \
    (1 << (pin1) | 1 << (pin2) | 1 << (pin3) | 1 << (pin4))


struct stepmotor_dev *stepmotor_new(int pin1, int pin2, int pin3,
                        int pin4, double step_angle, int reduction_ratio,
                        int pullin_freq, int pullout_freq, int flags)
{
    struct stepmotor_dev *dev;

    dev = xmalloc(sizeof(*dev));
    if (dev) {
        memset(dev, 0, sizeof(*dev));
        dev->pin1 = pin1;
        dev->pin2 = pin2;
        dev->pin3 = pin3;
        dev->pin4 = pin4;

#define FSEL_OUT(x) bcm2835_gpio_fsel(x, BCM2835_GPIO_FSEL_OUTP)
        FSEL_OUT(pin1);
        FSEL_OUT(pin2);
        FSEL_OUT(pin3);
        FSEL_OUT(pin4);

        dev->ev = event_new(base, -1, EV_PERSIST, cb_timer, dev);
        if (dev->ev == NULL) {
            free(dev);
            return NULL;
        }

        dev->pin_mask = PIN_MASK(pin1, pin2, pin3, pin4);
        dev->step_angle = step_angle;
        dev->reduction_ratio = reduction_ratio;
        dev->pullin_freq = pullin_freq;
        dev->pullout_freq = pullout_freq;
        INIT_PULSE_FOUR(dev->pulses_four, pin1, pin2, pin3, pin4);
        INIT_PULSE_DFOUR(dev->pulses_dfour, pin1, pin2, pin3, pin4);
        INIT_PULSE_EIGHT(dev->pulses_eight, pin1, pin2, pin3, pin4);
        /* XXX: for 8 pulses */
        dev->nr_cycle_pulse = (int)((double)reduction_ratio * 360 / step_angle);

        switch (flags) {
        case SMF_PULSE_FOUR:
            dev->pulse = dev->pulses_four;
            dev->nr_pulse = 4;
            dev->nr_cycle_pulse /= 2;
            break;
        case SMF_PULSE_DFOUR:
            dev->pulse = dev->pulses_dfour;
            dev->nr_pulse = 4;
            dev->nr_cycle_pulse /= 2;
            break;
        case SMF_PULSE_EIGHT:
        default:
            dev->pulse = dev->pulses_eight;
            dev->nr_pulse = 8;
            break;
        }

        dev->flags = flags;

        /* TODO init event */
    }
    return dev;
}

void stepmotor_del(struct stepmotor_dev *dev)
{
    if (dev) {
        if (dev->ev)
            eventfd_del(dev->ev);
        free(dev);
    }
}

static void cb_timer(int fd, short what, void *arg)
{
    struct stepmotor_dev *dev = arg;

    if (dev->angle > 0) {
        dev->pidx++;
        if (dev->pidx >= dev->nr_pulse)
            dev->pidx = 0;
    } else if (dev->angle < 0) {
        dev->pidx--;
        if (dev->pidx < 0)
            dev->pidx = dev->nr_pulse - 1;
    }
    bcm2835_gpio_write_mask(dev->pulse[dev->pidx], dev->pin_mask);

    /* finished ? */
    if (--dev->remain_pulses <= 0) {
        event_del(dev->ev);
        bcm2835_gpio_write_mask(0, dev->pin_mask);

        /* invoid done callback */
        if (dev->cb)
            dev->cb(dev, dev->opaque);
    }
}

int stepmotor(struct stepmotor_dev *dev, double angle,
        int delay/* ms */, __cb_stepmotor_done cb, void *opaque)
{
    struct timeval tv;
    double n;

    /* undo */
    if (delay == -1) {
        if (event_pending(dev->ev, EV_TIMEOUT, NULL)) {
            event_del(dev->ev);
            bcm2835_gpio_write_mask(0, dev->pin_mask);
            return 0;
        }
        return ENOENT;
    }

    if (delay < 1)
        delay = 1;
    dev->angle = angle;
    dev->delay = delay;
    n = angle / 360;
    n = n < 0 ? -n : n;
    dev->remain_pulses = (int)(n * dev->nr_cycle_pulse);
    dev->cb = cb;
    dev->opaque = opaque;
    tv.tv_sec = delay / 1000;
    tv.tv_usec = (delay % 1000) * 1000;
    if (evtimer_add(dev->ev, &tv) < 0)
        return -ENOSPC;
    return 0;
}
