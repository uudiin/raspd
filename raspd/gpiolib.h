#ifndef __GPIO_INT_H__
#define __GPIO_INT_H__

enum trigger_edge {
    edge_none,
    edge_rising,
    edge_falling,
    edge_both
};

int bcm2835_gpio_poll(unsigned int pin,
        enum trigger_edge edge, int timeout, int *valuep);

int bcm2835_gpio_signal(unsigned int pin, enum trigger_edge edge,
        int (*callback)(int nr, int value, void *opaque), void *opaque);

#endif /* __GPIO_INT_H__ */
