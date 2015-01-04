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

#define MODNAME     "tank"

/*
 * N.B. These have been reversed compared to Gert & Dom's original code!
 * This is because the transistor board I use for talking to the Heng
 * Long RX18 inverts the signal.  So the GPIO_SET pointer here actually
 * sets the GPIO pin low - but that ends up as a high at the tank.
 */
#define GPIO_CLR *(bcm2835_gpio + 7)
#define GPIO_SET *(bcm2835_gpio + 10)

/*
 * GPIO pin that connects to the Heng Long main board
 * (Pin 7 is the top right pin on the Pi's GPIO, next to the yellow video-out)
 */
#define PIN_DEFAULT	7

static int pin = PIN_DEFAULT;

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
static int machine_gun  = 0xFE440F78;
static int recoil       = 0xFE420F24;

static int current_code = 0;
static int speed = 0;
static struct event *ev = NULL;

struct code_entry { 
    TAILQ_ENTRY(code_entry) link;
    int code;
    int count;
};

TAILQ_HEAD(code_qh, code_entry);
struct code_qh code_qh = TAILQ_HEAD_INITIALIZER(code_qh);

struct code_env {
	int code;
	int step;
};

#define min(x, y) (((x) < (y)) ? (x) : (y))
#define max(x, y) (((x) > (y)) ? (x) : (y))

static void env_del(struct code_env *env)
{
    if (ev != NULL) {
        eventfd_del(ev);
        ev = NULL;
    }
    free(env);
}

static int get_code(void)
{
    int code = 0;
    struct code_entry *q = TAILQ_FIRST(&code_qh);
    if (q != NULL) {
        current_code = 0;
        if (q->count > 0) {
            q->count--;
            code = q->code;
        } else if (q->count == -1) {
            current_code = q->code;
            code = current_code;
        }

        if (q->count == 0) {
            TAILQ_REMOVE(&code_qh, q, link);
            free(q);
        }

    } else {
        code = current_code;
    }

    return code;
}

static void cb_send_bit(int fd, short what, void *arg)
{
    int tv_end = 0;
	struct timeval tv = {0, 250};
	struct code_env *env = arg;

    do {

        env->step++;
        if (env->step == 32 * 2 + 1) {

            /* Force a 4ms gap between messages */
            GPIO_CLR = 1 << pin;

            tv.tv_usec = 3333;
            env->code = get_code();
            env->step = 0;
            if (env->code == 0) {
                tv_end = 1;
            }
        } else {

            int bit, i, set;
            i = ((env->step - 1) / 2);
            bit = (env->code >> (31 - i)) & 0x1;
            set = env->step % 2;
            if (bit == 1) {
                if (set == 1) {
                    GPIO_SET = 1 << pin;
                } else {
                    GPIO_CLR = 1 << pin;
                }
            } else {
                if (set == 1) {
                    GPIO_CLR = 1 << pin;
                } else {
                    GPIO_SET = 1 << pin;
                }
            }
        }

    } while (0);

    if (tv_end != 0) {
        env_del(env);
    } else {
        evtimer_add(ev, &tv);
    }
}

/* Sends one individual code to the main tank controller */
void send_code(int code)
{
	struct code_env *env;
	struct timeval tv = {0, 500};

	/* Send header "bit" (not a valid Manchester code) */
	GPIO_SET = 1 << pin;

    env = xmalloc(sizeof(*env));
	if (env == NULL) {
		return;
	}
	env->code = code;
	env->step = 0;
    ev = evtimer_new(base, cb_send_bit, env);
    if (ev == NULL) {
        env_del(env);
        return;
    }

    if (evtimer_add(ev, &tv) < 0) {
        env_del(env);
        return;
    }
}

void insert_code(int code, int count)
{
	struct code_entry *entry;
        entry = xmalloc(sizeof(*entry));
	if (entry == NULL) {
		return;
	}
	entry->code = code;
	entry->count = count;
	TAILQ_INSERT_TAIL(&code_qh, entry, link);

    if (ev == NULL) {
        send_code(get_code());
    }
}

int tank_sdown_main(int wfd, int argc, char *argv[])
{
	speed--;
	speed = max(0, speed);
	if (speed == 0) {
		insert_code(ignition, 10);
		insert_code(idle, 40);
	}
	return 0;
}
DEFINE_MODULE(tank_sdown);

int tank_sup_main(int wfd, int argc, char *argv[])
{
	if (speed ==0) {
		
		insert_code(idle, 40);
		insert_code(ignition, 10);
		insert_code(idle, 300);
	}

	speed++;
	speed = min(2, speed);
	return 0;
}
DEFINE_MODULE(tank_sup);

int tank_brake_main(int wfd, int argc, char *argv[])
{
	insert_code(idle, 10);
	return 0;
}
DEFINE_MODULE(tank_brake);

int tank_fwd_main(int wfd, int argc, char *argv[])
{
	insert_code(idle, 10);
	if (speed == 1) {
		insert_code(fwd_slow, -1);
	} else if (speed == 2) {
		insert_code(fwd_fast, -1);
	}
	return 0;
}
DEFINE_MODULE(tank_fwd);

int tank_rev_main(int wfd, int argc, char *argv[])
{
	insert_code(idle, 10);
	if (speed == 1) {
		insert_code(rev_slow, -1);
	} else if (speed == 2) {
		insert_code(rev_fast, -1);
	}
	return 0;
}
DEFINE_MODULE(tank_rev);

int tank_left_main(int wfd, int argc, char *argv[])
{
	insert_code(idle, 10);
	if (speed == 1) {
		insert_code(left_slow, -1);
	} else if (speed == 2) {
		insert_code(left_fast, -1);
	}
	return 0;
}
DEFINE_MODULE(tank_left);

int tank_right_main(int wfd, int argc, char *argv[])
{
	insert_code(idle, 10);
	if (speed == 1) {
		insert_code(right_slow, -1);
	} else if (speed == 2) {
		insert_code(right_fast, -1);
	}
	return 0;
}
DEFINE_MODULE(tank_right);

int tank_turret_left_main(int wfd, int argc, char *argv[])
{
	insert_code(idle, 10);
	insert_code(turret_left, 25);
	insert_code(idle, 10);
	return 0;
}
DEFINE_MODULE(tank_turret_left);

int tank_turret_right_main(int wfd, int argc, char *argv[])
{
	insert_code(idle, 10);
	insert_code(turret_right, 25);
	insert_code(idle, 10);
	return 0;
}
DEFINE_MODULE(tank_turret_right);

int tank_turret_elev_main(int wfd, int argc, char *argv[])
{
	insert_code(idle, 10);
	insert_code(turret_elev, 50);
	insert_code(idle, 10);
	return 0;
}
DEFINE_MODULE(tank_turret_elev);

int tank_fire_main(int wfd, int argc, char *argv[])
{
	insert_code(idle, 10);
	insert_code(fire, 50);
	insert_code(idle, 10);
	return 0;
}
DEFINE_MODULE(tank_fire);

int tank_init(void)
{
	luaenv_getconf_int(MODNAME, "PIN", &pin);

	/* must use INP_GPIO before we can use OUT_GPIO */
	bcm2835_gpio_fsel(pin, BCM2835_GPIO_FSEL_INPT);
	bcm2835_gpio_fsel(pin, BCM2835_GPIO_FSEL_OUTP);

	GPIO_CLR = 1 << pin;

	return 0;
}

/*
 * DEFINE_MODULE(tank);
 */
static struct module __module_tank = {
    .name = "tank",
    .init = tank_init,
    .main = NULL,
};

static __init void __reg_module_tank(void)
{
    register_module(&__module_tank);
}
