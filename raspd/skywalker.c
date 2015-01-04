#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <getopt.h>
#include <time.h>
#include <event2/event.h>

#include <xmalloc.h>
#include <bcm2835.h>

#include "module.h"
#include "event.h"

#include "gpio.h"

#define INVALID_PIN(x)      ((x) < 0 || (x) >= NR_BCM2835_GPIO)

struct blink {
    double len;
    int count;
    int counted;
    int interval;
    struct event *timer;
    struct event *ev_done;
    int gpio;
};

static void trig_done(int fd, short what, void *arg)
{
    struct blink *bl = arg;
    bcm2835_gpio_write(bl->gpio, LOW);
    /* FIXME  need ? */
    /*evtimer_del(bl->ev_done);*/
}

static void cb_timer(int fd, short what, void *arg)
{
    struct blink *bl = arg;
    struct timeval tv;
    int pin;

    if (bl->count != -1 && bl->counted++ >= bl->count) {
        eventfd_del(bl->timer);
        eventfd_del(bl->ev_done);
        free(bl);
        return;
    }

    pin = bl->gpio;

    printf("cb_timer pin = %d\n", pin);
    bcm2835_gpio_write(pin, HIGH);
    /*
    usleep(bl->len);
    bcm2835_gpio_write(pin, LOW);
    */
    tv.tv_sec = bl->len / 1000000;
    tv.tv_usec = bl->len % 1000000;
    evtimer_add(bl->ev_done, &tv);
}

/*
 * usage:
 *   skywalker --ctrlnum [0-200] --count [num] --interval 20 18
 */
static int sw_main(int wfd, int argc, char *argv[])
{
    static struct option options[] = {
        { "ctrlnum",  required_argument, NULL, 'c' },
        { "count",    required_argument, NULL, 'n' },
        { "interval", required_argument, NULL, 't' },
        { 0, 0, 0, 0 }
    };
    int pin;
    int c;
    int count = -1;
    int interval = -1;  /* millisecond */
    int ctrl;

    while ((c = getopt_long(argc, argv, "a:u:n:t:", options, NULL)) != -1) {
        switch (c) {
        case 'c': ctrl = atoi(optarg); break;
        case 'n': count = atoi(optarg); break;
        case 't': interval = atoi(optarg); break;
        default:
            return 1;
        }
    }

    if (optind >= argc)
        return 1;

    pin = atoi(argv[optind]);
    bcm2835_gpio_fsel(pin, BCM2835_GPIO_FSEL_OUTP);

    if (count != -1 && interval != -1) {
        struct blink *bl;
        struct timeval timeout;
        int err = -EFAULT;

        do {
            bl = xmalloc(sizeof(*bl));
            memset(bl, 0, sizeof(*bl));
            bl->len = 1000 * (0.9 + ctrl * (2.1 - 0.9) / 200.0);
            bl->count = count;
            bl->interval = interval;
            bl->gpio = pin;
            timeout.tv_sec = interval / 1000;
            timeout.tv_usec = (interval % 1000) * 1000;

            err = -ENOMEM;
            bl->ev_done = evtimer_new(base, trig_done, bl);
            if (bl->ev_done == NULL)
                break;

            err = -EIO;
            if (register_timer(EV_PERSIST, &timeout, cb_timer, bl, &bl->timer) < 0)
                break;

            err = 0;
        } while (0);

        if (err != 0) {
            if (bl) {
                if (bl->ev_done)
                    event_free(bl->ev_done);
                if (bl->timer)
                    event_free(bl->timer);
                free(bl);
            }

            return 1;
        }
    }

    return 0;
}

DEFINE_MODULE(sw);
