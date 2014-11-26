#ifndef __GPIO_INT_H__
#define __GPIO_INT_H__

#include <event2/event.h>

enum trigger_edge {
    EDGE_none,
    EDGE_rising,
    EDGE_falling,
    EDGE_both
};

int bcm2835_gpio_poll(unsigned int pin,
        enum trigger_edge edge, int timeout, int *valuep);

int bcm2835_gpio_signal(unsigned int pin, enum trigger_edge edge,
                event_callback_fn cb, void *opaque, struct event **ev);

#endif /* __GPIO_INT_H__ */
