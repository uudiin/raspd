#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include <termios.h>

#include <xmalloc.h>
#include <bcm2835.h>

#include "module.h"
#include "luaenv.h"
#include "l298n.h"

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

static int pin_int1 = INT1_DEFAULT;
static int pin_int2 = INT2_DEFAULT;
static int pin_int3 = INT3_DEFAULT;
static int pin_int4 = INT4_DEFAULT;
static int pin_ena = ENA_DEFAULT;
static int pin_enb = ENB_DEFAULT;

static int s_max_speed = MAXSPEED_DEFAULT;
static int s_range = RANGE_DEFAULT;
static int s_pwm_div = PWMDIV_DEFAULT;
static int s_step = 0;

static int left_speed = 0;
static int right_speed = 0;
static int left_forward = 0;
static int right_forward = 0;

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
	s_step = s_range / s_max_speed;
	s_range = s_step * s_max_speed;

	bcm2835_gpio_fsel(pin_int1, BCM2835_GPIO_FSEL_OUTP);
	bcm2835_gpio_fsel(pin_int2, BCM2835_GPIO_FSEL_OUTP);
	bcm2835_gpio_fsel(pin_int3, BCM2835_GPIO_FSEL_OUTP);
	bcm2835_gpio_fsel(pin_int4, BCM2835_GPIO_FSEL_OUTP);

	bcm2835_gpio_fsel(pin_ena, pwm_ios[pin_ena].alt_fun);
	bcm2835_gpio_fsel(pin_enb, pwm_ios[pin_enb].alt_fun);

	bcm2835_pwm_set_clock(s_pwm_div);

	bcm2835_pwm_set_mode(pwm_ios[pin_ena].channel, 1, 1);
	bcm2835_pwm_set_range(pwm_ios[pin_ena].channel, s_range);

	bcm2835_pwm_set_mode(pwm_ios[pin_enb].channel, 1, 1);
	bcm2835_pwm_set_range(pwm_ios[pin_enb].channel, s_range);
}

static void l298n_forward_reverse(int speed, int intx, int inty, int enx, int step)
{
	int s = speed > 0 ? speed : -speed;

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
	bcm2835_pwm_set_data(pwm_ios[enx].channel, s * step); \
}

#define do_speedup(_fwd_, _l_, _in1_, _in2_, _enx_, _step_) { \
		int speed = (_fwd_) ? (_l_) : -(_l_); \
		l298n_forward_reverse(speed, _in1_, _in2_, _enx_, _step_); \
	}

int l298n_lup(struct l298n_dev *dev)
{
	dev->left_forward = 1;
	do_speedup(dev->left_forward, dev->left_speed,
            dev->in1, dev->in2, dev->ena, dev->step);
	return 0;
}

int l298n_lup_main(int wfd, int argc, char *argv[])
{
	left_forward = 1;
	do_speedup(left_forward, left_speed,
            pin_int1, pin_int2, pin_ena, s_step);
	return 0;
}

DEFINE_MODULE(l298n_lup);

int l298n_ldown(struct l298n_dev *dev)
{
	dev->left_forward = 0;
	do_speedup(dev->left_forward, dev->left_speed,
            dev->in1, dev->in2, dev->ena, dev->step);
	return 0;
}

int l298n_ldown_main(int wfd, int argc, char *argv[])
{
	left_forward = 0;
	do_speedup(left_forward, left_speed,
            pin_int1, pin_int2, pin_ena, s_step);
	return 0;
}

DEFINE_MODULE(l298n_ldown);

int l298n_lbrake(struct l298n_dev *dev)
{
	dev->left_speed = 0;
	do_speedup(dev->left_forward, dev->left_speed,
            dev->in1, dev->in2, dev->ena, dev->step);
	return 0;
}

int l298n_lbrake_main(int wfd, int argc, char *argv[])
{
	left_speed = 0;
	do_speedup(left_forward, left_speed,
            pin_int1, pin_int2, pin_ena, s_step);
	return 0;
}

DEFINE_MODULE(l298n_lbrake);

int l298n_lspeedup(struct l298n_dev *dev)
{
	dev->left_speed++;
	dev->left_speed = min(dev->max_speed, dev->left_speed);
	do_speedup(dev->left_forward, dev->left_speed,
            dev->in1, dev->in2, dev->ena, dev->step);
	return 0;
}

int l298n_lspeedup_main(int wfd, int argc, char *argv[])
{
	left_speed++;
	left_speed = min(s_max_speed, left_speed);
	do_speedup(left_forward, left_speed,
            pin_int1, pin_int2, pin_ena, s_step);
	return 0;
}

DEFINE_MODULE(l298n_lspeedup);

int l298n_lspeeddown(struct l298n_dev *dev)
{
	dev->left_speed--;
	dev->left_speed = max(0, dev->left_speed);
	do_speedup(dev->left_forward, dev->left_speed,
            dev->in1, dev->in2, dev->ena, dev->step);
	return 0;
}

int l298n_lspeeddown_main(int wfd, int argc, char *argv[])
{
	left_speed--;
	left_speed = max(0, left_speed);
	do_speedup(left_forward, left_speed,
            pin_int1, pin_int2, pin_ena, s_step);
	return 0;
}

DEFINE_MODULE(l298n_lspeeddown);

int l298n_rup(struct l298n_dev *dev)
{
	dev->right_forward = 1;
	do_speedup(dev->right_forward, dev->right_speed,
            dev->in3, dev->in4, dev->enb, dev->step);
	return 0;
}

int l298n_rup_main(int wfd, int argc, char *argv[])
{
	right_forward = 1;
	do_speedup(right_forward, right_speed,
            pin_int3, pin_int4, pin_enb, s_step);
	return 0;
}

DEFINE_MODULE(l298n_rup);

int l298n_rdown(struct l298n_dev *dev)
{
	dev->right_forward = 0;
	do_speedup(dev->right_forward, dev->right_speed,
            dev->in3, dev->in4, dev->enb, dev->step);
	return 0;
}

int l298n_rdown_main(int wfd, int argc, char *argv[])
{
	right_forward = 0;
	do_speedup(right_forward, right_speed,
            pin_int3, pin_int4, pin_enb, s_step);
	return 0;
}

DEFINE_MODULE(l298n_rdown);

int l298n_rbrake(struct l298n_dev *dev)
{
	dev->right_speed = 0;
	do_speedup(dev->right_forward, dev->right_speed,
            dev->in3, dev->in4, dev->enb, dev->step);
	return 0;
}

int l298n_rbrake_main(int wfd, int argc, char *argv[])
{
	right_speed = 0;
	do_speedup(right_forward, right_speed,
            pin_int3, pin_int4, pin_enb, s_step);
	return 0;
}

DEFINE_MODULE(l298n_rbrake);

int l298n_rspeedup(struct l298n_dev *dev)
{
	dev->right_speed++;
	dev->right_speed = min(dev->max_speed, dev->right_speed);
	do_speedup(dev->right_forward, dev->right_speed,
            dev->in3, dev->in4, dev->enb, dev->step);
	return 0;
}

int l298n_rspeedup_main(int wfd, int argc, char *argv[])
{
	right_speed++;
	right_speed = min(s_max_speed, right_speed);
	do_speedup(right_forward, right_speed,
            pin_int3, pin_int4, pin_enb, s_step);
	return 0;
}

DEFINE_MODULE(l298n_rspeedup);

int l298n_rspeeddown(struct l298n_dev *dev)
{
	dev->right_speed--;
	dev->right_speed = max(0, dev->right_speed);
	do_speedup(dev->right_forward, dev->right_speed,
            dev->in3, dev->in4, dev->enb, dev->step);
    return 0;
}

int l298n_rspeeddown_main(int wfd, int argc, char *argv[])
{
	right_speed--;
	right_speed = max(0, right_speed);
	do_speedup(right_forward, right_speed,
            pin_int3, pin_int4, pin_enb, s_step);
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
		case 'a': pin_int1 = atoi(optarg); break;
		case 'b': pin_int2 = atoi(optarg); break;
		case 'c': pin_int3 = atoi(optarg); break;
		case 'd': pin_int4 = atoi(optarg); break;
		case 'e': pin_ena = atoi(optarg); break;
		case 'f': pin_enb = atoi(optarg); break;
		case 'g': s_range = atoi(optarg); break;
		case 'h': s_max_speed = atoi(optarg); break;
		case 'i': s_pwm_div = atoi(optarg); break;
		}
	}

	l298n_init();

        fprintf(fd, "l298n_main end\n");
	return 0;
}

static int l298n_init_init(void)
{
    luaenv_getconf_int(MODNAME, "ENA", &pin_ena);
    luaenv_getconf_int(MODNAME, "ENB", &pin_enb);
    luaenv_getconf_int(MODNAME, "IN1", &pin_int1);
    luaenv_getconf_int(MODNAME, "IN2", &pin_int2);
    luaenv_getconf_int(MODNAME, "IN3", &pin_int3);
    luaenv_getconf_int(MODNAME, "IN4", &pin_int4);

	l298n_init();

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

struct l298n_dev *l298n_new(int ena, int enb, int in1, int in2, int in3, int in4,
                            int max_speed, int range, int pwm_div)
{
    struct l298n_dev *dev;

    dev = xmalloc(sizeof(*dev));
    if (dev) {
        memset(dev, 0, sizeof(*dev));
        dev->ena = ena;
        dev->enb = enb;
        dev->in1 = in1;
        dev->in2 = in2;
        dev->in3 = in3;
        dev->in4 = in4;

        dev->max_speed = max_speed;
        dev->range = range;
        dev->pwm_div = pwm_div;

        dev->step = range / max_speed;
        dev->range = dev->step * max_speed;

        bcm2835_gpio_fsel(dev->in1, BCM2835_GPIO_FSEL_OUTP);
        bcm2835_gpio_fsel(dev->in2, BCM2835_GPIO_FSEL_OUTP);
        bcm2835_gpio_fsel(dev->in3, BCM2835_GPIO_FSEL_OUTP);
        bcm2835_gpio_fsel(dev->in4, BCM2835_GPIO_FSEL_OUTP);

        bcm2835_gpio_fsel(dev->ena, pwm_ios[dev->ena].alt_fun);
        bcm2835_gpio_fsel(dev->enb, pwm_ios[dev->enb].alt_fun);

        bcm2835_pwm_set_clock(dev->pwm_div);

        bcm2835_pwm_set_mode(pwm_ios[dev->ena].channel, 1, 1);
        bcm2835_pwm_set_range(pwm_ios[dev->ena].channel, dev->range);

        bcm2835_pwm_set_mode(pwm_ios[dev->enb].channel, 1, 1);
        bcm2835_pwm_set_range(pwm_ios[dev->enb].channel, dev->range);
    }
    return dev;
}

void l298n_del(struct l298n_dev *dev)
{
    if (dev) {
        free(dev);
    }
}
