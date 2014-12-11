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
#include <xmalloc.h>
#include <event2/event.h>

#include "module.h"
#include "event.h"

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

#if 0
int main(int argc, char **argv)
{ 

	int g,rep,i;
	char inchar;

	if (!bcm2835_init())
		return 1;

	/* must use INP_GPIO before we can use OUT_GPIO */
	bcm2835_gpio_fsel(PIN, BCM2835_GPIO_FSEL_INPT);
	bcm2835_gpio_fsel(PIN, BCM2835_GPIO_FSEL_OUTP);

	GPIO_CLR = 1 << PIN;

	/* Send the idle and ignition codes */
	printf("Idle\n");
	for (i = 0; i < 40; i++) {
		sendCode(idle);
	}
	printf("Ignition\n");
	for (i = 0; i < 10; i++) {
		sendCode(ignition);
	}
	printf("Waiting for ignition\n");
	for (i = 0; i < 300; i++) {
		sendCode(idle);
	}

	/* Loop, sending movement commands indefinitely */
	do {
		printf("Ready: ");
		inchar = getchar();
		if (inchar == 'w') {
			printf("Forward\n");
			for (i = 0; i < 100; i++) {
				sendCode(fwd_fast);
			}
			sendCode(idle);
		} else if (inchar == 's') {
			printf("Reverse\n");
			for (i = 0; i < 100; i++) {
				sendCode(rev_fast);
			}
			sendCode(idle);
		} else if (inchar == 'a') {
			printf("Left\n");
			for (i = 0; i < 10; i++) {
				sendCode(left_fast);
			}
			sendCode(idle);
		} else if (inchar == 'd') {
			printf("Right\n");
			for (i = 0; i < 10; i++) {
				sendCode(right_fast);
			}
			sendCode(idle);
		} else if (inchar == 'q') {
			printf("Turret Left\n");
			for (i = 0; i < 25; i++) {
				sendCode(turret_left);
			}
			sendCode(idle);
		} else if (inchar == 'e') {
			printf("Turret Right\n");
			for (i = 0; i < 25; i++) {
				sendCode(turret_right);
			}
			sendCode(idle);
		} else if (inchar == 'z') {
			printf("Turret Elev\n");
			for (i = 0; i < 50; i++) {
				sendCode(turret_elev);
			}
			sendCode(idle);
		} else if (inchar == 'x') {
			printf("Fire\n");
			for (i = 0; i < 50; i++) {
				sendCode(fire);
			}
			sendCode(idle);
		}
	} while (inchar != '.');

	printf("Ignition Off\n");
	for (i = 0; i < 10; i++) {
		sendCode(ignition);
	}
	printf("Idle\n");
	for (i = 0; i < 40; i++) {
		sendCode(idle);
	}

	bcm2835_close();

	return 0;
}
#endif

struct code_env {
	int code;
	int count;
	struct event *ev_timer;
};

static free_env(struct code_env *env)
{
    if (env->ev_timer)
        eventfd_del(env->ev_timer);
    free(env);
}

static void cb_send_code(int fd, short what, void *arg)
{
	struct code_env *env = arg;
	printf("fuck %d\n", env->count);	
	env->count--;
	if (env->count == 0) {
		free_env(env);
		return;
	}
}

static void send_code(int code, int count)
{
        struct timeval tv = {0, 3333};
	struct code_env *env;

        env = xmalloc(sizeof(*env));
	if (env == NULL)
		return;
	memset(env, 0, sizeof(*env));

	env->code = code;
	env->count = count;

	if (register_timer(EV_PERSIST, &tv,
		cb_send_code, env, &env->ev_timer) < 0) {
			free_env(env);
	}
}

int tank_fwd_main(int wfd, int argc, char *argv[])
{
	return 0;
}
DEFINE_MODULE(tank_fwd);

int tank_rev_main(int wfd, int argc, char *argv[])
{
	return 0;
}
DEFINE_MODULE(tank_rev);

int tank_left_main(int wfd, int argc, char *argv[])
{
	return 0;
}
DEFINE_MODULE(tank_left);

int tank_right_main(int wfd, int argc, char *argv[])
{
	return 0;
}
DEFINE_MODULE(tank_right);

int tank_turret_left_main(int wfd, int argc, char *argv[])
{
	return 0;
}
DEFINE_MODULE(tank_turret_left);

int tank_turret_right_main(int wfd, int argc, char *argv[])
{
	return 0;
}
DEFINE_MODULE(tank_turret_right);

int tank_turret_elev_main(int wfd, int argc, char *argv[])
{
	return 0;
}
DEFINE_MODULE(tank_turret_elev);

int tank_fire_main(int wfd, int argc, char *argv[])
{
	send_code(fire, 5);
	return 0;
}
DEFINE_MODULE(tank_fire);

int tank_init_main(int wfd, int argc, char *argv[])
{
	int c;

	static struct option opts[] = {
		{ "pin", required_argument, NULL, 'p' },
		{ 0, 0, 0, 0 } };

	for (;;) {
		c = getopt_long(argc, argv, "p:", opts, NULL);
		if (c < 0) break;

		switch (c) {
		case 'p': pin = atoi(optarg); break;
		}
	}

	/* must use INP_GPIO before we can use OUT_GPIO */
	bcm2835_gpio_fsel(pin, BCM2835_GPIO_FSEL_INPT);
	bcm2835_gpio_fsel(pin, BCM2835_GPIO_FSEL_OUTP);

	GPIO_CLR = 1 << pin;

	return 0;
}
DEFINE_MODULE(tank_init);
