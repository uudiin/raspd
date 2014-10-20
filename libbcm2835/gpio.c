#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <stdint.h>

static uint32_t barrier_read(volatile uint32_t *addr)
{
    volatile uint32_t tmp;
    uint32_t retval = *addr;
    tmp = *addr;
    return retval;
}

static void barrier_write(volatile uint32_t *addr, uint32_t v)
{
    *addr = v;
    *addr = v;
}

static void set_bits(volatile uint32_t *addr, uint32_t v, uint32_t mask)
{
    uint32_t value = *addr;
    value = (value & ~mask) | (v & mask);
    barrier_write(addr, value);
}

#define VALID_PIN(x) \
    do { \
        if ((x) < 0 || (x) >= 54) \
            return -EINVAL; \
    } while (0)

void gpio_fsel(volatile uint32_t *gpio, unsigned int pin, enum gpio_mode mode)
{
    volatile uint32_t *addr = gpio + GPIO_FSEL0 / 4 + (pin / 10);
    int shift = (pin % 10) * 3;
    set_bits(addr, mode << shift, GPIO_FSEL_MASK << shift);
    return 0;
}

void gpio_set(volatile uint32_t *gpio, unsigned int pin)
{
    volatile uint32_t *addr = gpio + GPIO_SET0 / 4 + (pin / 32);
    int shift = pin % 32;
    barrier_write(addr, 1 << shift);
}

void gpio_clr(volatile uint32_t *gpio, unsigned int pin)
{
    volatile uint32_t *addr = gpio + GPIO_CLR0 / 4 + (pin / 32);
    int shift = pin % 32;
    barrier_write(addr, 1 << shift);
}

uint8_t gpio_level(volatile uint32_t *gpio, unsigned int pin)
{
    volatile uint32_t *addr = gpio + GPIO_LEV0 / 4 + (pin / 32);
    int shift = pin % 32;
    uint32_t v = barrier_read(addr);
    return (v & (1 << shift)) ? HIGH : LOW;
}

void gpio_write(...)
{
    /* FIXME */
    /* TODO */
}

