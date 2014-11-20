#ifndef __EVENT_H__
#define __EVENT_H__

#include <sys/time.h>
#include <event2/event.h>

#define PRIORITY_MAXIMUM    10

int eventfd_add(int fd, short flags, struct timeval *timeout,
        event_callback_fn cb, void *opaque, struct event **eventp);

void eventfd_del(struct event *ev);
int register_timer(short flags, struct timeval *timeout,
        event_callback_fn cb, void *opaque, struct event **eventp);

int register_signal(int signum, event_callback_fn cb, void *opaque);
int do_event_loop(void);
int event_init(void);
void event_exit(void);

#endif /* __EVENT_H__ */
