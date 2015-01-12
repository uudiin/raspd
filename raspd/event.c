#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <sched.h>
#include <sys/mman.h>

#include <event2/event.h>

#include "event.h"

struct event_base *evbase;

int eventfd_add(int fd, short flags, struct timeval *timeout,
        event_callback_fn cb, void *opaque, struct event **eventp)
{
    struct event *ev;

    ev = event_new(evbase, fd, flags, cb, opaque);
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

int rasp_event_loop(void)
{
    return event_base_dispatch(evbase);
}

int rasp_event_loopexit(void)
{
    return event_base_loopexit(evbase, NULL);
}

int rasp_event_init(void)
{
    evbase = event_base_new();
    if (evbase == NULL)
        return -ENOMEM;

    event_base_priority_init(evbase, PRIORITY_MAXIMUM);
    return 0;
}

void rasp_event_exit(void)
{
    /* FIXME */
    event_base_free(evbase);
}

int sched_realtime(void)
{
	struct sched_param sp;
	int err = 0;
	memset(&sp, 0, sizeof(sp));
	sp.sched_priority = sched_get_priority_max(SCHED_FIFO);
	if (sched_setscheduler(0, SCHED_FIFO, &sp) < 0)
		err = -EPERM;
	if (mlockall(MCL_CURRENT | MCL_FUTURE) < 0)
		err = -EACCES;
	return err;
}
