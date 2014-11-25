#include <stdio.h>
#include <stdint.h>
#include <getopt.h>

#include <bcm2835.h>

#include "module.h"

#define min(a, b)   (((a) < (b)) ? (a) : (b))
#define max(a, b)   (((a) > (b)) ? (a) : (b))

#define NR_CHANNDEL     2
#define NR_PWM_IO       4

struct pwm_channel {
    uint32_t range;
    int data;
    int initialized;
};

struct pwm_io_info {
    uint8_t gpio;
    uint8_t channel;
    uint8_t alt_fun;
    uint8_t j8_pin;
    int initialized;
};

static struct pwm_channel channels[NR_CHANNDEL];

static struct pwm_io_info pwm_ios[NR_PWM_IO] = {
    { 12, 0, BCM2835_GPIO_FSEL_ALT0, RPI_BPLUS_GPIO_J8_32 },
    { 13, 1, BCM2835_GPIO_FSEL_ALT0, RPI_BPLUS_GPIO_J8_33 },
    { 18, 0, BCM2835_GPIO_FSEL_ALT5, RPI_BPLUS_GPIO_J8_12 },
    { 19, 1, BCM2835_GPIO_FSEL_ALT5, RPI_BPLUS_GPIO_J8_35 },
};

static uint32_t clock;
static uint32_t step;

static pwm_main(int wfd, int argc, char *argv[])
{
    static struct option options[] = {
        { "range", required_argument, NULL, 'r' },
        { "clock", required_argument, NULL, 'c' },
        { "step",  required_argument, NULL, 't' },
        { "set",   required_argument, NULL, 's' },
        { "up",    no_argument,       NULL, 'u' },
        { "down",  no_argument,       NULL, 'd' },
        { 0, 0, 0, 0 }
    };
    int c;
    uint32_t range = 1000;
    uint32_t clk = BCM2835_PWM_CLOCK_DIVIDER_16;
    uint32_t _step = 100;
    int data = 0;
    int up = 0;
    int down = 0;

    while ((c = getopt_long(argc, argv, "r:c:t:s:ud", options, NULL)) != -1) {
        switch (c) {
        case 'r': range = (uint32_t)atoi(optarg); break;
        case 'c': clk = (uint32_t)atoi(optarg); break;
        case 't': _step = (uint32_t)atoi(optarg); break;
        case 's': data = atoi(optarg); break;
        case 'u': up = 1; break;
        case 'd': down = 1; break;
        default:
            return 1;
        }
    }

    /* check clock is equal 2^N */
    if ((clk & (clk - 1)) || clk <= 1)
        clk = BCM2835_PWM_CLOCK_DIVIDER_16;

    if (clock != clk) {
        clock = clk;
        bcm2835_pwm_set_clock(clock);
    }

    if (step != _step)
        step = _step;

    while (optind < argc) {
        int i;
        unsigned int io = atoi(argv[optind]);

        for (i = 0; i < NR_PWM_IO; i++) {
            if (io == pwm_ios[i].gpio) {
                int v = 0;
                int n = pwm_ios[i].channel;

                if (!pwm_ios[i].initialized) {
                    pwm_ios[i].initialized = 1;
                    bcm2835_gpio_fsel(io, pwm_ios[i].alt_fun);
                }

                if (!channels[n].initialized) {
                    channels[n].initialized = 1;
                    bcm2835_pwm_set_mode(n, 1, 1);
                }
                if (channels[n].range != range) {
                    bcm2835_pwm_set_range(n, range);
                    channels[n].range = range;
                }

                if (data != 0) {
                    v = min(data, channels[n].range);
                    v = max(v, 0);
                } else if (up) {
                    v = min(channels[n].data + step, channels[n].range);
                } else if (down) {
                    v = max(channels[n].data - step, 0);
                }

                bcm2835_pwm_set_data(n, v);
                channels[n].data = v;

                break;
            }
        }

        optind++;
    }

    return 0;
}

DEFINE_MODULE(pwm);
