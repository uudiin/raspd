#include <stdio.h>
#include <errno.h>
#include <bcm2835.h>

/* 5 6 13 19 */
#define PIN1  RPI_BPLUS_GPIO_J8_35
#define PIN2  RPI_BPLUS_GPIO_J8_33
#define PIN3  RPI_BPLUS_GPIO_J8_31
#define PIN4  RPI_BPLUS_GPIO_J8_29
#define PINMASK  ((1 << PIN1) | (1 << PIN2) | (1 << PIN3) | (1 << PIN4))

int main(int argc, char *argv[])
{
    static uint32_t pulse_four[] = {
        1 << PIN1,
        1 << PIN2,
        1 << PIN3,
        1 << PIN4
    };
    static uint32_t pulse_double_four[] = {
        (1 << PIN1) | (1 << PIN2),
        (1 << PIN2) | (1 << PIN3),
        (1 << PIN3) | (1 << PIN4),
        (1 << PIN4) | (1 << PIN1)
    };
    static uint32_t pulse_eight[] = {
        1 << PIN1,
        (1 << PIN1) | (1 << PIN2),
        1 << PIN2,
        (1 << PIN2) | (1 << PIN3),
        1 << PIN3,
        (1 << PIN3) | (1 << PIN4),
        1 << PIN4,
        (1 << PIN4) | (1 << PIN1)
    };
    int delay = 10;
    uint32_t *pulse = pulse_eight;
    int pulse_nr = 8;
    unsigned int total_pulse = -1;
    int i = 0;

    if (argc >= 2)
        delay = atoi(argv[1]);
    if (argc >= 3) {
        if (strcmp(argv[2], "s4") == 0) {
            pulse = pulse_four;
            pulse_nr = 4;
        } else if (strcmp(argv[2], "d4") == 0) {
            pulse = pulse_double_four;
            pulse_nr = 4;
        }
    }
    if (argc >= 4)
        total_pulse = atoi(argv[3]);

    if (!bcm2835_init())
        return 1;

#define FSEL(x) bcm2835_gpio_fsel(x, BCM2835_GPIO_FSEL_OUTP);
    FSEL(PIN1);
    FSEL(PIN2);
    FSEL(PIN3);
    FSEL(PIN4);

    while (total_pulse--) {
        bcm2835_gpio_write_mask(pulse[i], PINMASK);
        i++;
        if (i == pulse_nr)
            i = 0;

        bcm2835_delay(delay);
    }

    bcm2835_close();

    return 0;
}
