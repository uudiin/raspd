#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <getopt.h>
#include <termios.h>

#include <bcm2835.h>

#include "../raspd/softpwm.h"


static struct termios old, new;

static void init_termios(int echo) 
{
	tcgetattr(0, &old); /* grab old terminal i/o settings */
	new = old; /* make new settings same as old settings */
	new.c_lflag &= ~ICANON; /* disable buffered i/o */
	new.c_lflag &= echo ? ECHO : ~ECHO; /* set echo mode */
	tcsetattr(0, TCSANOW, &new); /* use these new terminal i/o settings now */
}

/* Restore old terminal i/o settings */
static void reset_termios(void) 
{
	tcsetattr(0, TCSANOW, &old);
}


static int min_data, max_data;
static int pin[4];
static int pwm_data[4];
static int nr_pin;

static void softpwm_set(int index, int incr)
{
    int i;
    int err;

    if (index > nr_pin || index > 3)
        return;

    if (incr == 0)
        pwm_data[index] = min_data;
    else
        pwm_data[index] += incr;

    if (pwm_data[index] < min_data)
        pwm_data[index] = min_data;
    if (pwm_data[index] > max_data)
        pwm_data[index] = max_data;

    err = softpwm_set_data(pin[index], pwm_data[index]);
    if (err < 0) {
        fprintf(stdout, "softpwm_set_data(), err = %d\n", err);
        return;
    }

    for (i = 0; i < nr_pin; i++)
        fprintf(stdout, "%d\t", pwm_data[i]);
    fprintf(stdout, "\n");
}

int main(int argc, char *argv[])
{
    int cycle = 10;     /* 10 ms */
    int step_us = 5;    /* 5 us */
    int min_us = 1000;  /* 1 ms */
    int max_us = 2000;  /* 2 ms */
    int full = 0;
    int inchar;
    int i;
    int err;
    static struct option options[] = {
        { "cycle", required_argument, NULL, 'c' },
        { "step",  required_argument, NULL, 's' },
        { "min",   required_argument, NULL, 'l' },
        { "max",   required_argument, NULL, 'h' },
        { "full",  no_argument,       NULL, 'u' },
        { 0, 0, 0, 0 }
    };
    int c;

    while ((c = getopt_long(argc, argv, "c:s:l:h:u", options, NULL)) != -1) {
        switch (c) {
        case 'c': cycle = atoi(optarg); break;
        case 's': step_us = atoi(optarg); break;
        case 'l': min_us = atoi(optarg); break;
        case 'h': max_us = atoi(optarg); break;
        case 'u': full = 1; break;
        default:
            return 1;
        }
    }

    min_data = min_us / step_us;
    max_data = max_us / step_us;

    fprintf(stdout, "\n"
        "config:\n"
        "  PWM frequency:  %d Hz\n"
        "  PWM steps:      %d\n"
        "  min:            %d (%d us)\n"
        "  max:            %d (%d us)\n"
        "\n",
        1000 / cycle, cycle * 1000 / step_us,
        min_data, min_us, max_data, max_us);

    init_termios(0);
    if (bcm2835_init() < 0)
        return 1;
    if ((err = softpwm_init(cycle * 1000, step_us, 0)) < 0) {
        fprintf(stderr, "softpwm_init(), err = %d\n", err);
        return 1;
    }

    for (i = 0; optind < argc; i++, optind++) {
        pin[i] = atoi(argv[optind]);
        bcm2835_gpio_fsel(pin[i], BCM2835_GPIO_FSEL_OUTP);
        nr_pin++;
        if (full)
            pwm_data[i] = max_data;
        else
            pwm_data[i] = min_data;

        //softpwm_set(i, 1);
    }

    do {
        inchar = fgetc(stdin);
        switch (inchar) {
        case 'q': softpwm_set(0, 0); break;
        case 'a': softpwm_set(0, 1); break;
        case 'z': softpwm_set(0, -1); break;
        case 'w': softpwm_set(1, 0); break;
        case 's': softpwm_set(1, 1); break;
        case 'x': softpwm_set(1, -1); break;
        case 'e': softpwm_set(2, 0); break;
        case 'd': softpwm_set(2, 1); break;
        case 'c': softpwm_set(2, -1); break;
        case 'r': softpwm_set(3, 0); break;
        case 'f': softpwm_set(3, 1); break;
        case 'v': softpwm_set(3, -1); break;
        }
    } while (inchar != '0');

    softpwm_exit();
    bcm2835_close();
    reset_termios();
    return 0;
}
