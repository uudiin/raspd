#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <bcm2835.h>

/* GPIO pin that connects to the Heng Long main board
   (Pin 7 is the top right pin on the Pi's GPIO, next to the yellow video-out) */
#define PIN 7

/* Heng Long tank bit-codes */
int idle = 0xFE40121C;
int ignition = 0xFE401294;
int left_slow = 0xFE400608;
int left_fast = 0xFE400010;
int right_slow = 0xFE401930;
int right_fast = 0xFE401E2C;
int fwd_slow = 0xFE200F34;
int fwd_fast = 0xFE000F3C;
int rev_slow = 0xFE580F08;
int rev_fast = 0xFE780F00;
int turret_left = 0xFE408F0C;
int turret_right = 0xFE410F28;
int turret_elev = 0xFE404F3C;
int fire = 0xFE442F34;
int machine_gun = 0xFE440F78;
int recoil = 0xFE420F24;

/* Sends one individual bit using Manchester coding
   1 = high-low, 0 = low-high */
void sendBit(int bit)
{
	if (bit == 1) {
		bcm2835_gpio_write(PIN, HIGH);
		usleep(250);
		bcm2835_gpio_write(PIN, LOW);
		usleep(250);
	} else {
		bcm2835_gpio_write(PIN, LOW);
		usleep(250);
		bcm2835_gpio_write(PIN, HIGH);
		usleep(250);
	}
}

/* Sends one individual code to the main tank controller */
void sendCode(int code) {
	/* Send header "bit" (not a valid Manchester code) */
	bcm2835_gpio_write(PIN, HIGH);
	usleep(500);

	/* Send the code itself, bit by bit using Manchester coding */
	int i;
	for (i = 0; i < 32; i++) {
		int bit = (code>>(31-i)) & 0x1;
		sendBit(bit);
	}

	/* Force a 4ms gap between messages */
	bcm2835_gpio_write(PIN, LOW);
	usleep(3333);
}

int main(int argc, char **argv)
{ 
	int g,rep,i;
	char inchar;

	if (!bcm2835_init())
		return 1;

	bcm2835_gpio_fsel(PIN, BCM2835_GPIO_FSEL_OUTP);
	bcm2835_gpio_write(PIN, LOW);

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
