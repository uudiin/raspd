#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <stdint.h>
#include <getopt.h>
#include <termios.h>
#include <bcm2835.h>

#define min(x, y) (((x) < (y)) ? (x) : (y))
#define max(x, y) (((x) > (y)) ? (x) : (y))

struct pwm_io_info {
    uint8_t gpio;
    uint8_t channel;
    uint8_t alt_fun;
};

static struct pwm_io_info pwm_ios[] = {
    { 12, 0, BCM2835_GPIO_FSEL_ALT0 },
    { 13, 1, BCM2835_GPIO_FSEL_ALT0 },
    { 18, 0, BCM2835_GPIO_FSEL_ALT5 },
    { 19, 1, BCM2835_GPIO_FSEL_ALT5 }
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

/* Read 1 character - echo defines echo mode */
static char getch_(int echo)
{
	char ch;
	init_termios(echo);
	ch = getchar();
	reset_termios();
	return ch;
}


/*
 * 50 Hz = 19.2MHz / 64 / 6000
 *      1 ms  ->  300, min_throttle
 *      2 ms  ->  600, max_throttle
 */

int main(int argc, char *argv[])
{
    static struct option options[] = {
        { "divider", required_argument, NULL, 'd' },
        { "hz",      required_argument, NULL, 'f' },
        { "min",     required_argument, NULL, 'l' },
        { "max",     required_argument, NULL, 'h' },
        { "full",    no_argument,       NULL, 'u' },
        { 0, 0, 0, 0 }
    };
    int c;
    int divider = BCM2835_PWM_CLOCK_DIVIDER_64;
    int hz = 50;               /* 50 Hz, 20 ms */
    int min_keep_time = 900;   /* 900 us,  270 */
    int max_keep_time = 2100;  /* 2100 us, 630 */
    int period;
    int range;
    int min_throttle;
    int max_throttle;
    int throttle;
    unsigned long channel = 0;
    int full_speed = 0;
    char inchar;

    init_termios(0);
    if (!bcm2835_init())
        return 1;
    pwm_ios_map_init();

    while ((c = getopt_long(argc, argv, "d:f:l:h:u", options, NULL)) != -1) {
        switch (c) {
        case 'd': divider = atoi(optarg); break;
        case 'f': hz = atoi(optarg); break;
        case 'l': min_keep_time = atoi(optarg); break;
        case 'h': max_keep_time = atoi(optarg); break;
        case 'u': full_speed = 1; break;
        default:
            fprintf(stderr, "invalid argument\n");
            return 1;
        }
    }

    if (optind >= argc) {
        fprintf(stderr, "must specify the pin\n");
        return 1;
    }

    bcm2835_pwm_set_clock(divider);
    period = 1000000 / hz;  /* us */
    range = 19200000 / divider / hz;
    min_throttle = range * min_keep_time / period;
    max_throttle = range * max_keep_time / period;

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

    if (full_speed)
        throttle = max_throttle;
    else
        throttle = min_throttle;

#define THROTTLE2US(x) ((x) * period / range)

    fprintf(stdout,
        "config:\n\n"
        "  HZ: %d\n"
        "  pwm divider: %d\n"
        "  pwm range: %d\n"
        "  throttle min: range = %d  (%d us)\n"
        "  throttle max: range = %d  (%d us)\n"
        "  throttle first: %d  (%d us)\n",
        hz, divider, range,
        min_throttle, THROTTLE2US(min_throttle),
        max_throttle, THROTTLE2US(max_throttle),
        throttle, THROTTLE2US(throttle)
        );

    do {
        int new_throttle = throttle;

        if (channel & 0b01)
            bcm2835_pwm_set_data(0, throttle);
        if (channel & 0b10)
            bcm2835_pwm_set_data(1, throttle);

        inchar = getch_(0);
        switch (inchar) {
        case 'k': new_throttle += 1; break;
        case 'j': new_throttle -= 1; break;
        case 'l': new_throttle += 5; break;
        case 'h': new_throttle -= 5; break;
        case 'u': new_throttle += 50; break;
        case 'd': new_throttle -= 50; break;
        case 'a': new_throttle = min_throttle; break;
        case 'z': new_throttle = max_throttle; break;
        }

        if (new_throttle != throttle) {
            throttle = min(new_throttle, max_throttle);
            throttle = max(throttle, min_throttle);
            fprintf(stdout, "throttle = %d, keep_time = %d us\n",
                    throttle, THROTTLE2US(throttle));
        }
    } while (inchar != '.');

    if (channel & 0b01)
        bcm2835_pwm_set_data(0, 0);
    if (channel & 0b10)
        bcm2835_pwm_set_data(1, 0);

    bcm2835_close();
    reset_termios();

    return 0;
}
