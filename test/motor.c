#include <stdio.h>
#include <errno.h>
#include <bcm2835.h>

/* 5 6 13 19 */
#define PIN1  5
#define PIN2  6
#define PIN3  13
#define PIN4  19
#define PINMASK  ((1 << PIN1) | (1 << PIN2) | (1 << PIN3) | (1 << PIN4))

int main(int argc, char *argv[])
{
    static uint32_t pins[] = { 1 << PIN1, 1 << PIN2, 1 << PIN3, 1 << PIN4 };
    int delay = 10;
    int i = 0;

    if (argc >= 2)
        delay = atoi(argv[1]);

    bcm2835_init();

    while (1) {
        bcm2835_gpio_write_mask(pins[i], PINMASK);
        i++;
        if (i == 4)
            i = 0;

        bcm2835_delay(delay);
    }

    bcm2835_close();

    return 0;
}
