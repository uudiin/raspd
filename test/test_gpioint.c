#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <assert.h>
#include <unistd.h>
#include <getopt.h>

#include <bcm2835.h>
#include "../raspd/event.h"
#include "../raspd/gpiolib.h"

static void gpio_interrupt(int fd, short what, void *arg)
{
    static int nr;
    unsigned int pin = (unsigned int)arg;
    int value = (int)bcm2835_gpio_lev(pin);
    printf("pin = %d, value = %d, nr = %d\n", pin, value, ++nr);
}

int main(int argc, char *argv[])
{
    static struct option options[] = {
        { "edge",    required_argument, NULL, 'e' },
        { "signal",  no_argument,       NULL, 's' },
        { "timeout", required_argument, NULL, 't' },
        { 0, 0, 0, 0 }
    };
    int c;
    char *edge = "both";
    int signal = 0;
    int timeout = -1;    /* ms */
    enum trigger_edge te;
    unsigned int pin;
    int value;
    int err;

    if (bcm2835_init() < 0)
        return 1;
    gpiolib_init();

    while ((c = getopt_long(argc, argv, "e:st:", options, NULL)) != -1) {
        switch (c) {
        case 'e': edge = optarg; break;
        case 's': signal = 1; break;
        case 't': timeout = atoi(optarg); break;
        default:
            return 1;
        }
    }

    if (strcmp(edge, "rising") == 0) {
        te = EDGE_rising;
    } else if (strcmp(edge, "falling") == 0) {
        te = EDGE_falling;
    } else if (strcmp(edge, "both") == 0) {
        te = EDGE_both;
    } else {
        fprintf(stderr, "invalid edge %s\n", edge);
        return 1;
    }

    if (optind >= argc) {
        fprintf(stderr, "must specify a pin\n");
        return 1;
    }

    pin = atoi(argv[optind]);
    if (pin < 0 || pin >= 54) {
        fprintf(stderr, "invalid pin %s\n", argv[optind]);
        return 1;
    }

    if (signal) {
        err = rasp_event_init();
        assert(err >= 0);

        err = bcm2835_gpio_signal(pin, te, gpio_interrupt, (void *)pin, NULL);
        assert(err >= 0);

        err = rasp_event_loop();
        assert(err == 0);

        rasp_event_exit();
    } else {
        while (1) {
            err = bcm2835_gpio_poll(pin, te, timeout, &value);
            if (err < 0)
                break;
            printf("value = %d\n", value);
        }
    }

    gpiolib_exit();
    bcm2835_close();

    return 0;
}
