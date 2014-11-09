#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <getopt.h>

#include <bcm2835.h>

#include "module.h"

#define NR_BCM2835_GPIO     54

struct gpio_info {
    uint8_t alt_fun;
    uint8_t level;
    uint8_t initialized;
};

static struct gpio_info gpios[NR_BCM2835_GPIO];

/*
 * --pin 5,28 --alt [o|i|0|1|2|3|4|5] --pull [0|1]
 */
static int gpio_main(int argc, char *argv[])
{
    static struct option options[] = {
        { "pin",  required_argument, NULL, 'p' },
        { "alt",  required_argument, NULL, 'a' },
        { "pull", required_argument, NULL, 'u' },
        { 0, 0, 0, 0 }
    };
    int c;
    char *pins = NULL;
    char *fsel = NULL;
    int level = -1;
    char *token, *saveptr;

    while ((c = getopt_long(argc, argv, "p:a:u:", options, NULL)) != -1) {
        switch (c) {
        case 'p': pins = optarg; break;
        case 'a': fsel = optarg; break;
        case 'u': level = atoi(optarg); break;
        }
    }

    if (pins == NULL)
        return EINVAL;

    for ( ; token = strtok_r(pins, ",", &saveptr); pins = NULL) {
        int pin = atoi(token);

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
            bcm2835_gpio_write(pin, level);
        }
    }

    return 0;
}

DEFINE_MODULE(gpio);
