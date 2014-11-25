#include <stdio.h>
#include <getopt.h>
#include <termios.h>
#include <bcm2835.h>

#include "module.h"

#define INT1_DEFAULT	17
#define INT2_DEFAULT	27
#define INT3_DEFAULT	22
#define INT4_DEFAULT	23

#define ENA_DEFAULT	18
#define ENB_DEFAULT	13

#define RANGE_DEFAULT	50000
#define MAXSPEED_DEFAULT 5
#define PWMDIV_DEFAULT	BCM2835_PWM_CLOCK_DIVIDER_16

static int int1 = INT1_DEFAULT;
static int int2 = INT2_DEFAULT;
static int int3 = INT3_DEFAULT;
static int int4 = INT4_DEFAULT;
static int ena = ENA_DEFAULT;
static int enb = ENB_DEFAULT;

static int left_speed = 0;
static int right_speed = 0;
static int left_forward = 0;
static int right_forward = 0;
static int max_speed = MAXSPEED_DEFAULT;
static int step = 0;
static int range = RANGE_DEFAULT;
static int pwm_div = PWMDIV_DEFAULT;

static int reinit = 1;

#define min(x, y) (((x) < (y)) ? (x) : (y))
#define max(x, y) (((x) > (y)) ? (x) : (y))

struct pwm_io_info {
	uint8_t channel;
	uint8_t alt_fun;
};

static struct pwm_io_info pwm_ios[41] = {
	[12] = { 0, BCM2835_GPIO_FSEL_ALT0 },
	[13] = { 1, BCM2835_GPIO_FSEL_ALT0 },
	[18] = { 0, BCM2835_GPIO_FSEL_ALT5 },
	[19] = { 1, BCM2835_GPIO_FSEL_ALT5 }
};

static void l298n_init(void)
{
	step = range / max_speed;
	range = step * max_speed;

	bcm2835_gpio_fsel(int1, BCM2835_GPIO_FSEL_OUTP);
	bcm2835_gpio_fsel(int2, BCM2835_GPIO_FSEL_OUTP);
	bcm2835_gpio_fsel(int3, BCM2835_GPIO_FSEL_OUTP);
	bcm2835_gpio_fsel(int4, BCM2835_GPIO_FSEL_OUTP);

	bcm2835_gpio_fsel(ena, pwm_ios[ena].alt_fun);
	bcm2835_gpio_fsel(enb, pwm_ios[enb].alt_fun);

	bcm2835_pwm_set_clock(pwm_div);

	bcm2835_pwm_set_mode(pwm_ios[ena].channel, 1, 1);
	bcm2835_pwm_set_range(pwm_ios[ena].channel, range);

	bcm2835_pwm_set_mode(pwm_ios[enb].channel, 1, 1);
	bcm2835_pwm_set_range(pwm_ios[enb].channel, range);
}

#define l298n_speedup_xx(_pin_) \
	{ \
		int s = speed > 0 ? speed : -speed; \
		bcm2835_pwm_set_data(pwm_ios[_pin_].channel, s * step); \
	}

static void l298n_forward_reverse(int speed, int intx, int inty, int enx)
{
	if (speed > 0) {
		bcm2835_gpio_write(intx, HIGH);
		bcm2835_gpio_write(inty, LOW);
	} else if (speed < 0) {
		bcm2835_gpio_write(intx, LOW);
		bcm2835_gpio_write(inty, HIGH);
	} else {
		bcm2835_gpio_write(intx, LOW);
		bcm2835_gpio_write(inty, LOW);
	}
	l298n_speedup_xx(enx);
}

#define do_speedup_left() { \
		int speed = (left_forward) ? left_speed : -left_speed; \
		l298n_forward_reverse(speed, int1, int2, ena); \
	}

#define do_speedup_right() { \
		int speed = (right_forward) ? right_speed : -right_speed; \
		l298n_forward_reverse(speed, int3, int4, enb); \
	}

int l298n_main(int wfd, int argc, char *argv[])
{
	int c;
	
        fprintf(stderr, "l298n_main start %d\n", argc);

	static struct option opts[] = {
		{ "int1",	required_argument, NULL, 'a' },
		{ "int2",	required_argument, NULL, 'b' },
		{ "int3",	required_argument, NULL, 'c' },
		{ "int4",	required_argument, NULL, 'd' },
		{ "ena",	required_argument, NULL, 'e' },
		{ "enb",	required_argument, NULL, 'f' },
		{ "range",	required_argument, NULL, 'g' },
		{ "maxspeed",	required_argument, NULL, 'h' },
		{ "pwmdiv",	required_argument, NULL, 'i' },
		{ "lup",	required_argument, NULL, 'j' },
		{ "ldown",	required_argument, NULL, 'k' },
		{ "lbrake",	required_argument, NULL, 'l' },
		{ "lspeed",	required_argument, NULL, 'm' },
		{ "rup",	required_argument, NULL, 'n' },
		{ "rdown",	required_argument, NULL, 'o' },
		{ "rbrake",	required_argument, NULL, 'p' },
		{ "rspeed",	required_argument, NULL, 'q' },
		{ 0, 0, 0, 0 } };

	for (;;) {
		c = getopt_long(argc, argv, "a:b:c:d:e:f:g:h:i:jklm:nopq:", opts, NULL);
		if (c < 0) break;

		switch (c) {
		case 'a': int1 = atoi(optarg); reinit = 1; break;
		case 'b': int2 = atoi(optarg); reinit = 1; break;
		case 'c': int3 = atoi(optarg); reinit = 1; break;
		case 'd': int4 = atoi(optarg); reinit = 1; break;
		case 'e': ena = atoi(optarg); reinit = 1; break;
		case 'f': enb = atoi(optarg); reinit = 1; break;
		case 'g': range = atoi(optarg); reinit = 1; break;
		case 'h': max_speed = atoi(optarg); reinit = 1; break;
		case 'i': pwm_div = atoi(optarg); reinit = 1; break;
		case 'j': /* lup */
			left_forward = 1;
			do_speedup_left();
			break;
		case 'k': /* ldown */
			left_forward = 0;
			do_speedup_left();
			break;
		case 'l': /* lbrake */
			left_speed = 0;
			do_speedup_left();
			break;
		case 'm': /* lspeed */
			left_speed = atoi(optarg);
			left_speed = min(max_speed, left_speed);
			do_speedup_left();
			break;
		case 'n': /* rup */
			right_forward = 1;
			do_speedup_right();
			break;
		case 'o': /* rdown */
			right_forward = 0;
			do_speedup_right();
			break;
		case 'p': /* rbrake */
			right_speed = 0;
			do_speedup_right();
			break;
		case 'q': /* rspeed */
			right_speed = atoi(optarg);
			right_speed = min(max_speed, right_speed);
			do_speedup_right();
			break;
		}
	}

	if (reinit) {
		bcm2835_init();
		l298n_init();
		reinit = 0;
	}

        fprintf(stderr, "l298n_main end\n");
	return 0;
}

DEFINE_MODULE(l298n);
