#include <stdio.h>
#include <stdlib.h>
#include <errno.h>

#include <event2/event.h>

static struct event_base *base;

static int eventfd_add(int fd, short flags, struct timeval *timeout,
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

static void eventfd_del(struct event *ev)
{
    if (ev) {
        event_del(ev);
        event_free(ev);
    }
}

static int __register_timer(struct timeval *timeout,
                int once, event_callback_fn cb, void *opaque)
{
    //
}

int event_loop(event_base *base)
{
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
