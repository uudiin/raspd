#include <stdio.h>
#include <getopt.h>
#include <termios.h>
#include <bcm2835.h>

#include "module.h"
#include "luaenv.h"

#define MODNAME "l298n"

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

int l298n_lup_main(int wfd, int argc, char *argv[])
{
	left_forward = 1;
	do_speedup_left();
	return 0;
}

DEFINE_MODULE(l298n_lup);

int l298n_ldown_main(int wfd, int argc, char *argv[])
{
	left_forward = 0;
	do_speedup_left();
	return 0;
}

DEFINE_MODULE(l298n_ldown);

int l298n_lbrake_main(int wfd, int argc, char *argv[])
{
	left_speed = 0;
	do_speedup_left();
	return 0;
}

DEFINE_MODULE(l298n_lbrake);

int l298n_lspeedup_main(int wfd, int argc, char *argv[])
{
	left_speed++;
	left_speed = min(max_speed, left_speed);
	do_speedup_left();
	return 0;
}

DEFINE_MODULE(l298n_lspeedup);

int l298n_lspeeddown_main(int wfd, int argc, char *argv[])
{
	left_speed--;
	left_speed = max(0, left_speed);
	do_speedup_left();
	return 0;
}

DEFINE_MODULE(l298n_lspeeddown);

int l298n_rup_main(int wfd, int argc, char *argv[])
{
	right_forward = 1;
	do_speedup_right();
	return 0;
}

DEFINE_MODULE(l298n_rup);

int l298n_rdown_main(int wfd, int argc, char *argv[])
{
	right_forward = 0;
	do_speedup_right();
	return 0;
}

DEFINE_MODULE(l298n_rdown);

int l298n_rbrake_main(int wfd, int argc, char *argv[])
{
	right_speed = 0;
	do_speedup_right();
	return 0;
}

DEFINE_MODULE(l298n_rbrake);

int l298n_rspeedup_main(int wfd, int argc, char *argv[])
{
	right_speed++;
	right_speed = min(max_speed, right_speed);
	do_speedup_right();
	return 0;
}

DEFINE_MODULE(l298n_rspeedup);

int l298n_rspeeddown_main(int wfd, int argc, char *argv[])
{
	right_speed--;
	right_speed = max(0, right_speed);
	do_speedup_right();
	return 0;
}

DEFINE_MODULE(l298n_rspeeddown);

int l298n_init_main(int wfd, int argc, char *argv[])
{
	int c;
	FILE *fd = fdopen(wfd, "wb");	

	setvbuf(fd, NULL, _IONBF, 0);
        fprintf(fd, "l298n_main start %d\n", argc);

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
		{ 0, 0, 0, 0 } };

	for (;;) {
		c = getopt_long(argc, argv, "a:b:c:d:e:f:g:h:i:", opts, NULL);
		if (c < 0) break;

		switch (c) {
		case 'a': int1 = atoi(optarg); break;
		case 'b': int2 = atoi(optarg); break;
		case 'c': int3 = atoi(optarg); break;
		case 'd': int4 = atoi(optarg); break;
		case 'e': ena = atoi(optarg); break;
		case 'f': enb = atoi(optarg); break;
		case 'g': range = atoi(optarg); break;
		case 'h': max_speed = atoi(optarg); break;
		case 'i': pwm_div = atoi(optarg); break;
		}
	}

	bcm2835_init();
	l298n_init();

        fprintf(fd, "l298n_main end\n");
	return 0;
}

static int l298n_init_init(void)
{
    luaenv_getconf_int(MODNAME, "ENA", &ena);
    luaenv_getconf_int(MODNAME, "ENB", &enb);
    luaenv_getconf_int(MODNAME, "IN1", &int1);
    luaenv_getconf_int(MODNAME, "IN2", &int2);
    luaenv_getconf_int(MODNAME, "IN3", &int3);
    luaenv_getconf_int(MODNAME, "IN4", &int4);

    return 0;
}

/*
 * DEFINE_MODULE(l298n_init);
 */
static struct module __module_l298n_init = {
    .name = "l298n_init",
    .init = l298n_init_init,
    .main = l298n_init_main
};

static __init void __reg_module_l298n_init(void)
{
    register_module(&__module_l298n_init);
}
