#ifndef __GPIO_INT_H__
#define __GPIO_INT_H__

#include <sys/time.h>

enum trigger_edge {
    edge_none,
    edge_rising,
    edge_falling,
    edge_both
};

int bcm2835_gpio_poll(unsigned int pin, enum trigger_edge edge,
                            struct timeval *timeout, int *valuep);

int bcm2835_gpio_signal(unsigned int pin, enum trigger_edge edge,
                int (*callback)(int value, void *opaque), void *opaque);

int bcm2835_gpio_int_init(void);
void bcm2835_gpio_int_exit(void);

#endif /* __GPIO_INT_H__ */
