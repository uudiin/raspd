#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <event2/event.h>

#include <bcm2835.h>

#include "module.h"
#include "event.h"
#include "luaenv.h"

struct esc_dev {
    int pin;
    int refresh_rate;  /* hz */
    int keep_time;
    int min_keep_time; /* us */
    int max_keep_time; /* us */
    struct event *ev_timer;
    struct event *ev_done;
};

static void trig_done(int fd, short what, void *arg)
{
    struct esc_dev *dev = arg;
    bcm2835_gpio_write(dev->pin, LOW);
}

static void timer_refresh(int fd, short what, void *arg)
{
    struct esc_dev *dev = arg;
    struct timeval tv;
    tv.tv_sec = env->keep_time / 1000000;
    tv.tv_usec = env->keep_time % 1000000;
    bcm2835_gpio_write(env->pin, HIGH);
    evtimer_add(env->ev_done, &tv);
}

struct esc_dev *esc_new(...)
{
}

void esc_del(struct esc_dev *dev)
{
}

int esc_get(struct esc_dev *dev)
{
}

int esc_set(struct esc_dev *dev, int keep_time)
{
}

int esc_start(struct esc_dev *dev)
{
}

int esc_stop(struct esc_dev *dev)
{
}
