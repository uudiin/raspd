#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <getopt.h>

#include <xmalloc.h>
#include <bcm2835.h>

#include "module.h"
#include "event.h"

#include "gpio.h"

#define NR_BCM2835_GPIO     54
#define INVALID_PIN(x)      ((x) < 0 || (x) >= NR_BCM2835_GPIO)

struct gpio_info {
    uint8_t alt_fun;
    uint8_t level;
    uint8_t initialized;
};

static struct gpio_info gpios[NR_BCM2835_GPIO];

struct blink {
    int count;
    int counted;
    int interval;
    struct event *timer;
    int nr;
    int gpio[NR_BCM2835_GPIO];
};

static void cb_timer(int fd, short what, void *arg)
{
    struct blink *bl = arg;
    int i;

    if (bl->count != -1 && bl->counted++ >= bl->count) {
        eventfd_del(bl->timer);
        free(bl);
        return;
    }

    for (i = 0; i < bl->nr; i++) {
        int pin = bl->gpio[i];

        if (INVALID_PIN(pin))
            continue;
        gpios[pin].level ^= 1;
        bcm2835_gpio_write(pin, gpios[pin].level);
    }
}

int gpio_blink_multi(int gpio[], int nr, int count, int interval)
{
    struct blink *bl;
    struct timeval timeout;
    int i;
    int err = -EFAULT;

    if (nr > NR_BCM2835_GPIO)
        return -EINVAL;

    bl = xmalloc(sizeof(*bl));
    memset(bl, 0, sizeof(*bl));
    bl->count = count;
    bl->interval = interval;
    bl->nr = nr;
    timeout.tv_sec = interval / 1000;
    timeout.tv_usec = (interval % 1000) * 1000;

    for (i = 0; i < nr; i++) {
        if (INVALID_PIN(gpio[i])) {
            bl->gpio[i] = -1;   /* set invalid */
            continue;
        }
        bl->gpio[i] = gpio[i];

        bcm2835_gpio_fsel(gpio[i], BCM2835_GPIO_FSEL_OUTP);
        bcm2835_gpio_write(gpio[i], 1);
        gpios[gpio[i]].level = 1;
    }

    if ((err = register_timer(EV_PERSIST, &timeout,
                        cb_timer, bl, &bl->timer)) < 0) {
        free(bl);
        return err;
    }

    return 0;
}

int gpio_blink(int gpio, int count, int interval)
{
    return gpio_blink_multi(&gpio, 1, count, interval);
}

/*
 * usage:
 *   gpio --alt [o|i|0|1|2|3|4|5] --pull [0|1] 18 25
 */
static int gpio_main(int wfd, int argc, char *argv[])
{
    static struct option options[] = {
        { "alt",      required_argument, NULL, 'a' },
        { "pull",     required_argument, NULL, 'u' },
        { "count",    required_argument, NULL, 'n' },
        { "interval", required_argument, NULL, 't' },
        { 0, 0, 0, 0 }
    };
    int c;
    char *fsel = NULL;
    int level = -1;
    int count = -1;
    int interval = -1;  /* millisecond */
    int i;

    while ((c = getopt_long(argc, argv, "a:u:n:t:", options, NULL)) != -1) {
        switch (c) {
        case 'a': fsel = optarg; break;
        case 'u': level = atoi(optarg); break;
        case 'n': count = atoi(optarg); break;
        case 't': interval = atoi(optarg); break;
        default:
            return 1;
        }
    }

    if (optind >= argc)
        return 1;

    for (i = optind; i < argc; i++) {
        int pin = atoi(argv[i]);

        if (pin < 0 || pin > NR_BCM2835_GPIO)
            continue;

        if (fsel != NULL) {
            switch (*fsel) {
            case 'o': gpios[pin].alt_fun = BCM2835_GPIO_FSEL_OUTP; break;
            case 'i': gpios[pin].alt_fun = BCM2835_GPIO_FSEL_INPT; break;
            case '0': gpios[pin].alt_fun = BCM2835_GPIO_FSEL_ALT0; break;
            case '1': gpios[pin].alt_fun = BCM2835_GPIO_FSEL_ALT1; break;
            case '2': gpios[pin].alt_fun = BCM2835_GPIO_FSEL_ALT2; break;
            case '3': gpios[pin].alt_fun = BCM2835_GPIO_FSEL_ALT3; break;
            case '4': gpios[pin].alt_fun = BCM2835_GPIO_FSEL_ALT4; break;
            case '5': gpios[pin].alt_fun = BCM2835_GPIO_FSEL_ALT5; break;
            }
            gpios[pin].initialized = 1;
            bcm2835_gpio_fsel(pin, gpios[pin].alt_fun);
        }
        if (level == 0 || level == 1) {
            if (!gpios[pin].initialized) {
                gpios[pin].alt_fun = BCM2835_GPIO_FSEL_OUTP;
                gpios[pin].initialized = 1;
                bcm2835_gpio_fsel(pin, BCM2835_GPIO_FSEL_OUTP);
            }
            gpios[pin].level = level;
            bcm2835_gpio_write(pin, level);
        }
    }

    if (count != -1 && interval != -1) {
        struct blink *bl;
        struct timeval timeout;
        int err = -EFAULT;

        do {
            bl = xmalloc(sizeof(*bl));
            memset(bl, 0, sizeof(*bl));
            bl->count = count;
            bl->interval = interval;
            for (i = 0; optind < argc && i < NR_BCM2835_GPIO; optind++, i++)
                bl->gpio[i] = atoi(argv[optind]);
            bl->nr = i;
            timeout.tv_sec = interval / 1000;
            timeout.tv_usec = (interval % 1000) * 1000;

            err = -EIO;
            if (register_timer(EV_PERSIST, &timeout, cb_timer, bl, &bl->timer) < 0)
                break;

            err = 0;
        } while (0);

        if (err != 0) {
            if (bl)
                free(bl);

            return 1;
        }
    }

    return 0;
}

DEFINE_MODULE(gpio);
