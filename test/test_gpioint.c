#include <stdio.h>
#include <errno.h>
#include <assert.h>

#include <gpio-int.h>

static int gpio_interrupt(int value, void *opaque)
{
    unsigned int pin = (unsigned int)opaque;
    printf("pin = %d, value = %d\n", pin, value);
    _exit(0);
}

int main(int argc, char *argv[])
{
    static struct option options[] = {
        { "edge",    required_argument, NULL, 'e' },
        { "signal",  no_augument,       NULL, 's' },
        { "timeout", required_argument, NULL, 't' },
        { 0, 0, 0, 0 }
    };
    int c;
    char *edge = "both";
    int signal = 0;
    int timeout = 0;    /* ms */
    struct timeval tv;
    enum trigger_edge te;
    unsigned int pin;
    int value;
    int err;

    if (bcm2835_gpio_int_init() < 0)
        return 1;

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
        te = edge_rising;
    } else if (strcmp(edge, "falling") == 0) {
        te = edge_falling;
    } else if (strcmp(edge, "both") == 0) {
        te = edge_both;
    } else {
        fprintf(stderr, "invalid edge %s\n", edge);
        return 1;
    }

    if (timeout) {
        tv.tv_sec = timeout / 1000;
        tv.tv_usec = (timeout % 1000) * 1000;
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
        err = bcm2835_gpio_signal(pin, te, gpio_interrupt, (void *)pin);
        sleep(9999);
    } else {
        err = bcm2835_gpio_poll(pin, te, timeout ? &tv : NULL, &value);
        assert(err = 0);
        printf("value = %d\n", value);
    }

    return 0;
}
