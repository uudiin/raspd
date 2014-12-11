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
static int idle		= 0xFE40121C;
static int ignition	= 0xFE401294;
static int left_slow	= 0xFE400608;
static int left_fast	= 0xFE400010;
static int right_slow	= 0xFE401930;
static int right_fast	= 0xFE401E2C;
static int fwd_slow	= 0xFE200F34;
static int fwd_fast	= 0xFE000F3C;
static int rev_slow	= 0xFE580F08;
static int rev_fast	= 0xFE780F00;
static int turret_left	= 0xFE408F0C;
static int turret_right	= 0xFE410F28;
static int turret_elev	= 0xFE404F3C;
static int fire		= 0xFE442F34;
static int machine_gun	= 0xFE440F78;
static int recoil	= 0xFE420F24;

static int current_code = 0;
static int speed = 0;
static struct event *ev_timer = NULL;

struct code_entry { 
	TAILQ_ENTRY(code_entry) link; 
	int code;
	int count;
};

TAILQ_HEAD(code_qh, code_entry);

struct code_qh code_qh = TAILQ_HEAD_INITIALIZER(code_qh);

#define min(x, y) (((x) < (y)) ? (x) : (y))
#define max(x, y) (((x) > (y)) ? (x) : (y))

/*
 * Sends one individual bit using Manchester coding
 * 1 = high-low, 0 = low-high
 */
void sendBit(int bit)
{

	if (bit == 1) {
		GPIO_SET = 1 << pin;
		usleep(250);
		GPIO_CLR = 1 << pin;
		usleep(250);
	} else {
		GPIO_CLR = 1 << pin;
		usleep(250);
		GPIO_SET = 1 << pin;
		usleep(250);
	}
}

/* Sends one individual code to the main tank controller */
void sendCode(int code)
{
	/* Send header "bit" (not a valid Manchester code) */
	GPIO_SET = 1 << pin;
	usleep(500);

	/* Send the code itself, bit by bit using Manchester coding */
	int i;
	for (i = 0; i < 32; i++) {
		int bit = (code >> (31 - i)) & 0x1;
		sendBit(bit);
	}

	/* Force a 4ms gap between messages */
	GPIO_CLR = 1 << pin;
	usleep(3333);
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
}

static void cb_send_code(int fd, short what, void *arg)
{
	int code = 0;
	struct code_entry *q = TAILQ_FIRST(&code_qh);
	if (q != NULL) {
		current_code = 0;
		if (q->count > 0) {
			q->count--;
			code = q->code;
		} else {
			if (q->count == -1) {
				current_code = q->code;
				code = current_code;
			}
			TAILQ_REMOVE(&code_qh, q, link);
			free(q);
		}
	} else {
		code = current_code;
	}

	if (code != 0) {
		sendCode(code);
	}
}

static void start_timer(void)
{
	struct timeval tv = {0, 3333};
	register_timer(EV_PERSIST, &tv,
		cb_send_code, NULL, &ev_timer);
}

int tank_sdown_main(int wfd, int argc, char *argv[])
{
	speed--;
	speed = max(0, speed);
	return 0;
}
DEFINE_MODULE(tank_sdown);

int tank_sup_main(int wfd, int argc, char *argv[])
{
	speed++;
	speed = min(2, speed);
	return 0;
}
DEFINE_MODULE(tank_sup);

int tank_fwd_main(int wfd, int argc, char *argv[])
{
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
	insert_code(turret_left, 25);
	return 0;
}
DEFINE_MODULE(tank_turret_left);

int tank_turret_right_main(int wfd, int argc, char *argv[])
{
	insert_code(turret_right, 25);
	return 0;
}
DEFINE_MODULE(tank_turret_right);

int tank_turret_elev_main(int wfd, int argc, char *argv[])
{
	insert_code(turret_elev, 50);
	return 0;
}
DEFINE_MODULE(tank_turret_elev);

int tank_fire_main(int wfd, int argc, char *argv[])
{
	insert_code(fire, 50);
	return 0;
}
DEFINE_MODULE(tank_fire);

int tank_init_main(int wfd, int argc, char *argv[])
{
	luaenv_getconf_int(MODNAME, "PIN", &pin);

	/* must use INP_GPIO before we can use OUT_GPIO */
	bcm2835_gpio_fsel(pin, BCM2835_GPIO_FSEL_INPT);
	bcm2835_gpio_fsel(pin, BCM2835_GPIO_FSEL_OUTP);
	printf("pin = %d\n", pin);

	GPIO_CLR = 1 << pin;

	start_timer();

	return 0;
}
DEFINE_MODULE(tank_init);
