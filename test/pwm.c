#include <stdio.h>
#include <assert.h>
#include <stdint.h>
#include <getopt.h>
#include <bcm2835.h>

struct pwm_io_info {
    uint8_t gpio;
    uint8_t channel;
    uint8_t alt_fun;
    uint8_t j8_pin;
};

static struct pwm_io_info pwm_ios[] = {
    { 12, 0, BCM2835_GPIO_FSEL_ALT0, RPI_BPLUS_GPIO_J8_32 },
    { 13, 1, BCM2835_GPIO_FSEL_ALT0, RPI_BPLUS_GPIO_J8_33 },
    { 18, 0, BCM2835_GPIO_FSEL_ALT5, RPI_BPLUS_GPIO_J8_12 },
    { 19, 1, BCM2835_GPIO_FSEL_ALT5, RPI_BPLUS_GPIO_J8_35 }
};

static unsigned int pwm_ios_map[20];

#define MAP_NR (sizeof(pwm_ios_map) / sizeof(unsigned int))

static void pwm_ios_map_init(void)
{
    int i;
    for (i = 0; i < 20; i++)
        pwm_ios_map[i] = -1;
    pwm_ios_map[12] = 0;
    pwm_ios_map[13] = 1;
    pwm_ios_map[18] = 2;
    pwm_ios_map[19] = 3;
}

/*
 * pwm [options] io1 io2 ...
 *   e.g. pwm [--range XX --max XX --delay XX --clock XX] io1 io2 ...
 */
int main(int argc, char *argv[])
{
    static struct option options[] = {
        { "range", required_argument, NULL, 'r' },
        { "skew",  required_argument, NULL, 's' },
        { "min",   required_argument, NULL, 'l' },
        { "max",   required_argument, NULL, 'm' },
        { "delay", required_argument, NULL, 'd' },
        { "clock", required_argument, NULL, 'c' },
        { "count", required_argument, NULL, 'n' },
        { 0, 0, 0, 0 }
    };
    int c;
    uint32_t range = 1024;
    uint32_t skewing = 100;
    unsigned int min = 0;
    unsigned int max = 1024;
    unsigned int delay = 10;
    uint32_t clock = BCM2835_PWM_CLOCK_DIVIDER_16;
    int count = -1;
    int direction0 = 0;
    int direction1 = 0;
    int data0 = 0;
    int data1 = 0;
    int phase_diff;
    unsigned long channel = 0;

    if (!bcm2835_init())
        return 1;
    pwm_ios_map_init();

    while ((c = getopt_long(argc, argv, "r:s:l:m:d:c:n:", options, NULL)) != -1) {
        switch (c) {
        /* FIXME  use strtol */
        case 'r': range = atoi(optarg); break;
        case 's': skewing = atoi(optarg); break;
        case 'l': min = atoi(optarg); break;
        case 'm': max = atoi(optarg); break;
        case 'd': delay = atoi(optarg); break;
        case 'c': clock = atoi(optarg); break;
        case 'n': count = atoi(optarg); break;
        }
    }

    if (optind >= argc) {
        fprintf(stderr, "must specify the pin\n");
        return 1;
    }

    if (min < 0)
        min = 0;
    if (max > range)
        max = range;
    /* check clock is equal 2^N */
    if ((clock & (clock - 1)) || clock <= 1)
        clock = BCM2835_PWM_CLOCK_DIVIDER_16;

    bcm2835_pwm_set_clock(clock);

    while (optind < argc) {
        unsigned int io = atoi(argv[optind]);

        if (io < MAP_NR && pwm_ios_map[io] != -1) {
            int index = pwm_ios_map[io];
            struct pwm_io_info *info = &pwm_ios[index];

            assert(io == info->gpio);
            bcm2835_gpio_fsel(info->gpio, info->alt_fun);

            bcm2835_pwm_set_mode(info->channel, 1, 1);
            bcm2835_pwm_set_range(info->channel, range);

            channel |= (1 << info->channel);
        }

        optind++;
    }

    /* calc phase_difference */
    phase_diff = max * skewing / 100;

    direction0 = direction1 = 1;
    data0 = data1 = min;
    if ((channel & 0b11) == 0b11) {
        data1 = data0 - phase_diff;
        if (data1 < 0) {
            data1 = -data1;
            direction1 = -1;
        }
    }

#define UPDATE_DATA(n)               \
    do {                             \
        if (data ## n == 0)          \
            direction ## n = 1;      \
        else if (data ## n == max)   \
            direction ## n = -1;     \
        data ## n += direction ## n; \
    } while (0)

    count *= max;
    while (count--) {
        if (channel & 0b01) {
            bcm2835_pwm_set_data(0, data0);
            UPDATE_DATA(0);
        }
        if (channel & 0b10) {
            bcm2835_pwm_set_data(1, data1);
            UPDATE_DATA(1);
        }

        bcm2835_delay(delay);
    }

    bcm2835_close();
    return 0;
}
