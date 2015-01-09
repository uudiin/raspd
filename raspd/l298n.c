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

int l298n_ldown(struct l298n_dev *dev)
{
	dev->left_forward = 0;
	do_speedup(dev->left_forward, dev->left_speed,
            dev->in1, dev->in2, dev->ena, dev->step);
	return 0;
}

int l298n_lbrake(struct l298n_dev *dev)
{
	dev->left_speed = 0;
	do_speedup(dev->left_forward, dev->left_speed,
            dev->in1, dev->in2, dev->ena, dev->step);
	return 0;
}

int l298n_lspeedup(struct l298n_dev *dev)
{
	dev->left_speed++;
	dev->left_speed = min(dev->max_speed, dev->left_speed);
	do_speedup(dev->left_forward, dev->left_speed,
            dev->in1, dev->in2, dev->ena, dev->step);
	return 0;
}

int l298n_lspeeddown(struct l298n_dev *dev)
{
	dev->left_speed--;
	dev->left_speed = max(0, dev->left_speed);
	do_speedup(dev->left_forward, dev->left_speed,
            dev->in1, dev->in2, dev->ena, dev->step);
	return 0;
}

int l298n_rup(struct l298n_dev *dev)
{
	dev->right_forward = 1;
	do_speedup(dev->right_forward, dev->right_speed,
            dev->in3, dev->in4, dev->enb, dev->step);
	return 0;
}

int l298n_rdown(struct l298n_dev *dev)
{
	dev->right_forward = 0;
	do_speedup(dev->right_forward, dev->right_speed,
            dev->in3, dev->in4, dev->enb, dev->step);
	return 0;
}

int l298n_rbrake(struct l298n_dev *dev)
{
	dev->right_speed = 0;
	do_speedup(dev->right_forward, dev->right_speed,
            dev->in3, dev->in4, dev->enb, dev->step);
	return 0;
}

int l298n_rspeedup(struct l298n_dev *dev)
{
	dev->right_speed++;
	dev->right_speed = min(dev->max_speed, dev->right_speed);
	do_speedup(dev->right_forward, dev->right_speed,
            dev->in3, dev->in4, dev->enb, dev->step);
	return 0;
}

int l298n_rspeeddown(struct l298n_dev *dev)
{
	dev->right_speed--;
	dev->right_speed = max(0, dev->right_speed);
	do_speedup(dev->right_forward, dev->right_speed,
            dev->in3, dev->in4, dev->enb, dev->step);
    return 0;
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

#define DEFINE_L298N_CMD(_name_) \
    int _name_ ## _main(int wfd, int argc, char *argv[]) \
    { \
        struct l298n_dev *dev = luaenv_getdev(MODNAME); \
        if (dev == NULL) \
            return 1; \
        _name_ (dev); \
        return 0; \
    } \
    DEFINE_MODULE(_name_);

DEFINE_L298N_CMD(l298n_lup);
DEFINE_L298N_CMD(l298n_ldown);
DEFINE_L298N_CMD(l298n_lbrake);
DEFINE_L298N_CMD(l298n_lspeedup);
DEFINE_L298N_CMD(l298n_lspeeddown);
DEFINE_L298N_CMD(l298n_rup);
DEFINE_L298N_CMD(l298n_rdown);
DEFINE_L298N_CMD(l298n_rbrake);
DEFINE_L298N_CMD(l298n_rspeedup);
DEFINE_L298N_CMD(l298n_rspeeddown);

static void set_speed(int speed, int intx, int inty, int enx)
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
	bcm2835_pwm_set_data(pwm_ios[enx].channel, speed > 0 ? speed : -speed);
}

void l298n_set(struct l298n_dev *dev, int left_speed, int right_speed)
{
    if (dev->left_speed != left_speed) {
        dev->left_speed = min(left_speed, dev->range);
        dev->left_speed = max(dev->left_speed, -dev->range);
        set_speed(dev->left_speed, dev->in1, dev->in2, dev->ena);
    }
    if (dev->right_speed != right_speed) {
        dev->right_speed = min(right_speed, dev->range);
        dev->right_speed = max(dev->right_speed, -dev->range);
        set_speed(dev->right_speed, dev->in3, dev->in4, dev->enb);
    }
}

void l298n_get(struct l298n_dev *dev, int *left_speed, int *right_speed)
{
    if (left_speed)
        *left_speed = dev->left_speed;
    if (right_speed)
        *right_speed = dev->right_speed;
}

void l298n_change(struct l298n_dev *dev, int left, int right)
{
    int new_speed;
    if (left) {
        new_speed = dev->left_speed + left;
        new_speed = min(new_speed, dev->range);
        dev->left_speed = max(new_speed, -dev->range);
        set_speed(dev->left_speed, dev->in1, dev->in2, dev->ena);
    }
    if (right) {
        new_speed = dev->right_speed + right;
        new_speed = min(new_speed, dev->range);
        dev->right_speed = max(new_speed, -dev->range);
        set_speed(dev->right_speed, dev->in3, dev->in4, dev->enb);
    }
}
