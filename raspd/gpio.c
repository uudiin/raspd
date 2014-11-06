#include <stdio.h>
#include <errno.h>

#include <bcm2835.h>

#include "module.h"

/*
 * pin alt=[o|i|0|1|2|3|4|5] pull=[0|1] delay=N(ms)
 */
static void gpio_event(const char *cmd)
{
    unsigned char gpio = 0;
    char *str, *token, *saveptr;
    char *v;

    for (str = cmd; token = strtok_r(str, " \t,", &saveptr); str = NULL) {
        if (gpio && (strncmp(token, "alt", 3) == 0)) {
            int fsel = -1;

            v = strchr(token, '=');
            if (v == NULL)
                continue;

            switch (*++v) {
            case 'o': fsel = BCM2835_GPIO_FSEL_OUTP; break;
            case 'i': fsel = BCM2835_GPIO_FSEL_INPT; break;
            case '0': fsel = BCM2835_GPIO_FSEL_ALT0; break;
            case '1': fsel = BCM2835_GPIO_FSEL_ALT1; break;
            case '2': fsel = BCM2835_GPIO_FSEL_ALT2; break;
            case '3': fsel = BCM2835_GPIO_FSEL_ALT3; break;
            case '4': fsel = BCM2835_GPIO_FSEL_ALT4; break;
            case '5': fsel = BCM2835_GPIO_FSEL_ALT5; break;
            }
            bcm2835_gpio_fsel(gpio, fsel);

        } else if (gpio && (strncmp(token, "pull", 4) == 0)) {
            int value = 1;

            if (fsel == -1)
                bcm2835_gpio_fsel(gpio, BCM2835_GPIO_FSEL_OUTP);

            if (fsel == BCM2835_GPIO_FSEL_OUTP || fsel == -1) {
                v = strchr(token, '=');
                if (v == NULL)
                    continue;

                switch (*++v) {
                case '0': value = 0; break;
                case '1': value = 1; break;
                }

                bcm2835_gpio_write(gpio, value);
            }
        } else if (strncmp(token, "delay", 5) == 0) {
            int delay = 0;

            v = strchr(token, '=');
            if (v == NULL)
                continue;

            delay = atoi(++v);

            if (delay)
                bcm2835_delay(delay);
        } else {
            gpio = atoi(token);
        }
    }
}

static struct module gpio_mod = {
    .name = "gpio",
    .event = gpio_event
};

static __init void gpio_init(void)
{
    rgeister_module(&gpio_mod);
}

static __exit void gpio_exit(void)
{
}
