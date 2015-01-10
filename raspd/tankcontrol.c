/*
 * Raspberry Tank SSH Remote Control script
 * Ian Renton, June 2012
 * http://ianrenton.com
 * 
 * Based on the GPIO example by Dom and Gert
 * (http://elinux.org/Rpi_Low-level_peripherals#GPIO_Driving_Example_.28C.29)
 * Using Heng Long op codes discovered by ancvi_pIII
 * (http://www.rctanksaustralia.com/forum/viewtopic.php?p=1314#p1314)
*/

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <getopt.h>

#include <bcm2835.h>
#include <queue.h>
#include <xmalloc.h>
#include <event2/event.h>

#include "module.h"
#include "event.h"
#include "luaenv.h"

#include "tankcontrol.h"

#define MODNAME "tank"

/*
 * N.B. These have been reversed compared to Gert & Dom's original code!
 * This is because the transistor board I use for talking to the Heng
 * Long RX18 inverts the signal.  So the GPIO_SET pointer here actually
 * sets the GPIO pin low - but that ends up as a high at the tank.
 */
#define GPIO_CLR *(bcm2835_gpio + 7)
#define GPIO_SET *(bcm2835_gpio + 10)

/* Heng Long tank bit-codes */
static int idle         = 0xFE40121C;
static int ignition     = 0xFE401294;
static int left_slow	= 0xFE400608;
static int left_fast	= 0xFE400010;
static int right_slow	= 0xFE401930;
static int right_fast	= 0xFE401E2C;
static int fwd_slow     = 0xFE200F34;
static int fwd_fast     = 0xFE000F3C;
static int rev_slow     = 0xFE580F08;
static int rev_fast     = 0xFE780F00;
static int turret_left  = 0xFE408F0C;
static int turret_right = 0xFE410F28;
static int turret_elev  = 0xFE404F3C;
static int fire         = 0xFE442F34;
/*
static int machine_gun  = 0xFE440F78;
static int recoil       = 0xFE420F24;
*/

#define min(x, y) (((x) < (y)) ? (x) : (y))
#define max(x, y) (((x) > (y)) ? (x) : (y))

static int get_code(struct tank_dev *dev)
{
    int code = 0;
    struct code_entry *q = TAILQ_FIRST(&dev->qh);
    if (q != NULL) {
        dev->current_code = 0;
        if (q->count > 0) {
            q->count--;
            code = q->code;
        } else if (q->count == -1) {
            dev->current_code = q->code;
            code = dev->current_code;
        }

        if (q->count <= 0) {
            TAILQ_REMOVE(&dev->qh, q, link);
            free(q);
        }

    } else {
        code = dev->current_code;
    }

    return code;
}

static void cb_send_bit(int fd, short what, void *arg)
{
    int tv_end = 0;
	struct timeval tv = {0, 250};
	struct tank_dev *dev = arg;

    do {

        dev->step++;
        if (dev->step == 32 * 2 + 1) {

            /* Force a 4ms gap between messages */
            GPIO_CLR = 1 << dev->pin;

            tv.tv_usec = 3333;
            dev->code = get_code(dev);
            dev->step = 0;
            if (dev->code == 0) {
                tv_end = 1;
            }
        } else {

            int bit, i, set;
            i = ((dev->step - 1) / 2);
            bit = (dev->code >> (31 - i)) & 0x1;
            set = dev->step % 2;
            if (bit == 1) {
                if (set == 1) {
                    GPIO_SET = 1 << dev->pin;
                } else {
                    GPIO_CLR = 1 << dev->pin;
                }
            } else {
                if (set == 1) {
                    GPIO_CLR = 1 << dev->pin;
                } else {
                    GPIO_SET = 1 << dev->pin;
                }
            }
        }

    } while (0);

    if (tv_end != 0) {
        eventfd_del(dev->ev);
    } else {
        evtimer_add(dev->ev, &tv);
    }
}

/* Sends one individual code to the main tank controller */
void send_code(struct tank_dev *dev, int code)
{
	struct timeval tv = {0, 500};

	/* Send header "bit" (not a valid Manchester code) */
	GPIO_SET = 1 << dev->pin;

	dev->code = code;
	dev->step = 0;

    evtimer_add(dev->ev, &tv);
}

static void insert_code(struct tank_dev *dev, int code, int count)
{
	struct code_entry *entry;
        entry = xmalloc(sizeof(*entry));
	if (entry == NULL) {
		return;
	}
	entry->code = code;
	entry->count = count;
	TAILQ_INSERT_TAIL(&dev->qh, entry, link);

    if (dev->ev == NULL) {
        send_code(dev, get_code(dev));
    }
}

struct tank_dev *tank_new(int pin)
{
    struct tank_dev *dev;

    dev = malloc(sizeof(*dev));
    if (dev) {
        memset(dev, 0, sizeof(*dev));
        dev->pin = pin;

        dev->qh.tqh_last = &dev->qh.tqh_first;
        dev->ev = evtimer_new(evbase, cb_send_bit, dev);
        if (dev->ev == NULL) {
            free(dev);
            return NULL;
        }

        /* must use INP_GPIO before we can use OUT_GPIO */
        bcm2835_gpio_fsel(dev->pin, BCM2835_GPIO_FSEL_INPT);
        bcm2835_gpio_fsel(dev->pin, BCM2835_GPIO_FSEL_OUTP);

        GPIO_CLR = 1 << dev->pin;
    }

    return dev;
}

void tank_del(struct tank_dev *dev)
{
    if (dev) {
        if (dev->ev)
            eventfd_del(dev->ev);
        free(dev);
    }
}

void tank_sdown(struct tank_dev *dev)
{
	dev->speed--;
	dev->speed = max(0, dev->speed);
	if (dev->speed == 0) {
		insert_code(dev, ignition, 10);
		insert_code(dev, idle, 40);
	}
}

void tank_sup(struct tank_dev *dev)
{
	if (dev->speed ==0) {
		insert_code(dev, idle, 40);
		insert_code(dev, ignition, 10);
		insert_code(dev, idle, 300);
	}

	dev->speed++;
	dev->speed = min(2, dev->speed);
}

void tank_brake(struct tank_dev *dev)
{
	insert_code(dev, idle, 10);
}

void tank_fwd(struct tank_dev *dev)
{
	insert_code(dev, idle, 10);
	if (dev->speed == 1) {
		insert_code(dev, fwd_slow, -1);
	} else if (dev->speed == 2) {
		insert_code(dev, fwd_fast, -1);
	}
}

void tank_rev(struct tank_dev *dev)
{
	insert_code(dev, idle, 10);
	if (dev->speed == 1) {
		insert_code(dev, rev_slow, -1);
	} else if (dev->speed == 2) {
		insert_code(dev, rev_fast, -1);
	}
}

void tank_left(struct tank_dev *dev)
{
	insert_code(dev, idle, 10);
	if (dev->speed == 1) {
		insert_code(dev, left_slow, -1);
	} else if (dev->speed == 2) {
		insert_code(dev, left_fast, -1);
	}
}

void tank_right(struct tank_dev *dev)
{
	insert_code(dev, idle, 10);
	if (dev->speed == 1) {
		insert_code(dev, right_slow, -1);
	} else if (dev->speed == 2) {
		insert_code(dev, right_fast, -1);
	}
}

void tank_turret_left(struct tank_dev *dev)
{
	insert_code(dev, idle, 10);
	insert_code(dev, turret_left, 25);
	insert_code(dev, idle, 10);
}

void tank_turret_right(struct tank_dev *dev)
{
	insert_code(dev, idle, 10);
	insert_code(dev, turret_right, 25);
	insert_code(dev, idle, 10);
}

void tank_turret_elev(struct tank_dev *dev)
{
	insert_code(dev, idle, 10);
	insert_code(dev, turret_elev, 50);
	insert_code(dev, idle, 10);
}

void tank_fire(struct tank_dev *dev)
{
	insert_code(dev, idle, 10);
	insert_code(dev, fire, 50);
	insert_code(dev, idle, 10);
}

#define DEFINE_TANK_CMD(_name_) \
    int _name_ ## _main(int wfd, int argc, char *argv[]) \
    { \
        struct tank_dev *dev = luaenv_getdev(MODNAME); \
        if (dev == NULL) \
            return 1; \
        _name_ (dev); \
        return 0; \
    } \
    DEFINE_MODULE(_name_);

DEFINE_TANK_CMD(tank_sdown);
DEFINE_TANK_CMD(tank_sup);
DEFINE_TANK_CMD(tank_brake);
DEFINE_TANK_CMD(tank_fwd);
DEFINE_TANK_CMD(tank_rev);
DEFINE_TANK_CMD(tank_left);
DEFINE_TANK_CMD(tank_right);
DEFINE_TANK_CMD(tank_turret_left);
DEFINE_TANK_CMD(tank_turret_right);
DEFINE_TANK_CMD(tank_turret_elev);
DEFINE_TANK_CMD(tank_fire);
