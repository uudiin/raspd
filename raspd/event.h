#ifndef __EVENT_H__
#define __EVENT_H__

#include <sys/time.h>
#include <event2/event.h>

#define PRIORITY_MAXIMUM    10

extern struct event_base *base;

int eventfd_add(int fd, short flags, struct timeval *timeout,
        event_callback_fn cb, void *opaque, struct event **eventp);

void eventfd_del(struct event *ev);
int register_timer(short flags, struct timeval *timeout,
        event_callback_fn cb, void *opaque, struct event **eventp);

int register_signal(int signum, event_callback_fn cb, void *opaque);
int rasp_event_loop(void);
int rasp_event_loopexit(void);
int rasp_event_init(void);
void rasp_event_exit(void);

#endif /* __EVENT_H__ */
