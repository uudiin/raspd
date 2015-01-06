#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <time.h>
#include <event2/event.h>

#include <bcm2835.h>

#include "module.h"
#include "event.h"
#include "luaenv.h"
#include "esc.h"


static void trig_done(int fd, short what, void *arg)
{
    struct esc_dev *dev = arg;
    bcm2835_gpio_write(dev->pin, LOW);
}

static void timer_refresh(int fd, short what, void *arg)
{
    struct esc_dev *dev = arg;
    struct timeval tv;
    tv.tv_sec = dev-> throttle_time / 1000000;
    tv.tv_usec = dev-> throttle_time % 1000000;
    bcm2835_gpio_write(dev->pin, HIGH);
    evtimer_add(dev->ev_throttle, &tv);
}

static void cb_esc_started(int fd, short what, void *arg)
{
    struct esc_dev *dev = arg;
    if (dev->cb_started)
        dev->cb_started(dev, dev->opaque);
}

struct esc_dev *esc_new(int pin, int refresh_rate, int start_time,
                        int min_throttle_time, int max_throttle_time)
{
    struct esc_dev *dev;

    dev = malloc(sizeof(*dev));
    if (dev) {
        memset(dev, 0, sizeof(*dev));
        dev->pin = pin;
        dev->refresh_rate = refresh_rate;
        dev->min_throttle_time = min_throttle_time;
        dev->max_throttle_time = max_throttle_time;
        dev->start_time = start_time;
        dev->throttle_time = min_throttle_time;
        dev->period = 1000000 / refresh_rate; /* us */

        dev->ev_throttle= evtimer_new(evbase, trig_done, dev);
        if (dev->ev_throttle == NULL) {
            esc_del(dev);
            return NULL;
        }
        dev->ev_timer = event_new(evbase, -1, EV_PERSIST, timer_refresh, dev);
        if (dev->ev_timer == NULL) {
            esc_del(dev);
            return NULL;
        }

        bcm2835_gpio_fsel(pin, BCM2835_GPIO_FSEL_OUTP);
    }
    return dev;
}

void esc_del(struct esc_dev *dev)
{
    if (dev) {
        if (dev->ev_timer)
            eventfd_del(dev->ev_timer);
        if (dev->ev_throttle)
            eventfd_del(dev->ev_throttle);
        if (dev->ev_started)
            eventfd_del(dev->ev_started);
        free(dev);
    }
}

int esc_get(struct esc_dev *dev)
{
    return dev->throttle_time;
}

int esc_set(struct esc_dev *dev, int throttle_time)
{
    if (throttle_time < dev->min_throttle_time
            || throttle_time > dev->max_throttle_time)
        return -EINVAL;

    dev->throttle_time = throttle_time;
    return 0;
}

int esc_start(struct esc_dev *dev, __cb_esc_started cb_started, void *opaque)
{
    struct timeval tv;

    if (evtimer_pending(dev->ev_timer, NULL))
        return EEXIST;
    /* us */
    tv.tv_sec = dev->period / 1000000;
    tv.tv_usec = dev->period % 1000000;
    if (evtimer_add(dev->ev_timer, &tv) < 0)
        return -ENOSPC;

    /* set started callback */
    if (cb_started) {
        if (dev->ev_started == NULL)
            dev->ev_started = evtimer_new(evbase, cb_esc_started, dev);
        if (dev->ev_started == NULL)
            return -ENOMEM;
        dev->cb_started = cb_started;
        dev->opaque = opaque;
        /* ms */
        tv.tv_sec = dev->start_time / 1000;
        tv.tv_usec = (dev->start_time % 1000) * 1000;
        if (evtimer_add(dev->ev_started, &tv) < 0)
            return -EIO;
    }
    return 0;
}

int esc_stop(struct esc_dev *dev)
{
    if (!evtimer_pending(dev->ev_timer, NULL))
        return ENOENT;
    if (event_del(dev->ev_timer) < 0)
        return -ENOSPC;
    return 0;
}
