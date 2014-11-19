#include <stdio.h>
#include <stdlib.h>
#include <errno.h>

#include <event2/event.h>

#include "event.h"

static struct event_base *base;

int eventfd_add(int fd, short flags, struct timeval *timeout,
        event_callback_fn cb, void *opaque, struct event **eventp)
{
    struct event *ev;

    ev = event_new(base, fd, flags, cb, opaque);
    if (ev == NULL)
        return -ENOMEM;

    if (event_add(ev, timeout) < 0) {
        event_free(ev);
        return -EIO;
    }

    if (eventp)
        *eventp = ev;

    return 0;
}

void eventfd_del(struct event *ev)
{
    if (ev) {
        event_del(ev);
        event_free(ev);
    }
}

int register_timer(short flags, struct timeval *timeout,
        event_callback_fn cb, void *opaque, struct event **eventp)
{
    return eventfd_add(-1, flags, timeout, cb, opaque, eventp);
}

int register_signal(int signum, event_callback_fn cb, void *opaque)
{
    return eventfd_add(signum, EV_SIGNAL | EV_PERSIST,
                                NULL, cb, opaque, NULL);
}

int do_event_loop(void)
{
    return event_base_dispatch(base);
}

int event_init(void)
{
    base = event_base_new();
    if (base == NULL)
        return -ENOMEM;

    return 0;
}

void event_exit(void)
{
    /* FIXME */
}
