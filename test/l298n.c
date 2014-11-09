#include <stdio.h>
#include <getopt.h>
#include <termios.h>
#include <bcm2835.h>

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

static void do_speedup_left(int speed)
{
	left_speed = speed;
	l298n_forward_reverse(speed, int1, int2, ena);
}

static void do_speedup_right(int speed)
{
	right_speed = speed;
	l298n_forward_reverse(speed, int3, int4, enb);
}

static void do_forward(int speedup)
{
	int speed = max(left_speed, right_speed);
	if (speed == 0) speedup = 1;
	if (speedup) {
		if ((left_speed == right_speed)) {
			(speed < max_speed) ? speed++ : speed;
		}
	} else {
		speed = (speed < 0) ? -speed : speed;
	}

	left_speed = speed;
	right_speed = speed;

	do_speedup_left(speed);
	do_speedup_right(speed);
}

static void do_reverse(int speedup)
{
	int speed = min(left_speed, right_speed);
	if (speed == 0) speedup = 1;
	if (speedup) {
		if ((left_speed == right_speed)) {
			(speed > -max_speed) ? speed-- : speed;
		}
	} else {
		speed = (speed > 0) ? -speed : speed;
	}

	left_speed = speed;
	right_speed = speed;

	do_speedup_left(speed);
	do_speedup_right(speed);
}

static void do_left(void)
{
	int speed = max(left_speed, right_speed);

	left_speed = -speed;
	right_speed = speed;

	do_speedup_left(left_speed);
	do_speedup_right(right_speed);
}

static void do_right(void)
{
	int speed = max(left_speed, right_speed);

	left_speed = speed;
	right_speed = -speed;

	do_speedup_left(left_speed);
	do_speedup_right(right_speed);
}

static void do_speedup(void)
{
	if (left_speed > 0) {
		left_speed++;
		(left_speed > max_speed) ? max_speed : left_speed;
	} else if (left_speed < 0) {
		left_speed--;
		(left_speed < -max_speed) ? -max_speed : left_speed;
	}	

	if (right_speed > 0) {
		right_speed++;
		(right_speed > max_speed) ? max_speed : right_speed;
	} else if (right_speed < 0) {
		right_speed--;
		(right_speed < -max_speed) ? -max_speed : right_speed;
	}

	do_speedup_left(left_speed);
	do_speedup_right(right_speed);
}

static void do_speeddown(void)
{
	if (left_speed > 0) {
		left_speed--;
		(left_speed == 0) ? 0 : left_speed;
	} else if (left_speed < 0) {
		left_speed++;
		(left_speed == 0) ? 0 : left_speed;
	}	

	if (right_speed > 0) {
		right_speed--;
		(right_speed == 0) ? 0 : right_speed;
	} else if (right_speed < 0) {
		right_speed++;
		(right_speed == 0) ? 0 : right_speed;
	}

	do_speedup_left(left_speed);
	do_speedup_right(right_speed);
}
static void do_brake(void)
{
	left_speed = 0;
	right_speed = 0;

	do_speedup_left(0);
	do_speedup_right(0);
}

#define do_forward_xx(_r_) \
	{ \
		int speed = (_r_ ## _speed < max_speed) ? \
				(_r_ ## _speed + 1) : _r_ ## _speed; \
		do_speedup_ ## _r_ (speed); \
	}

#define do_reverse_xx(_r_) \
	{ \
		int speed = (_r_ ## _speed > -max_speed) ? \
				(_r_ ## _speed - 1) : _r_ ## _speed; \
		do_speedup_ ## _r_ (speed); \
	}

#define do_brake_xx(_r_) \
	{ \
		_r_ ## _speed = 0; \
		do_speedup_ ## _r_ (0); \
	}

static void usage(void)
{
	printf("usage:\n"
		"  l298n [options]\n");

	printf("options:\n"
		"  --int1	= int1 gpio number. default: %d\n"
		"  --int2	= int2 gpio number. default: %d\n"
		"  --int3	= int3 gpio number. default: %d\n"
		"  --int4	= int4 gpio number. default: %d\n"
		"  --ena	= ena gpio number. default: %d\n"
		"  --enb	= enb gpio number. default: %d\n"
		"  --range	= pwm range. default: %d\n"
		"  --maxspeed	= max speed value. default: %d\n"
		"  --pwmdiv	= pwmdiv: 32768, ... 16, 8, 4, 2, 1. default: %d\n"
		"  --help	= show this help text\n",
		INT1_DEFAULT, INT2_DEFAULT, INT3_DEFAULT, INT4_DEFAULT,
		ENA_DEFAULT, ENB_DEFAULT, RANGE_DEFAULT, MAXSPEED_DEFAULT,
		PWMDIV_DEFAULT);
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

int main(int argc, char *argv[])
{
	int c;
	int pr_usage = 0;
	char inchar;
	
	static struct option opts[] = {
		{ "int1",	required_argument, NULL, 'w' },
		{ "int2",	required_argument, NULL, 'x' },
		{ "int3",	required_argument, NULL, 'y' },
		{ "int4",	required_argument, NULL, 'z' },
		{ "ena",	required_argument, NULL, 'a' },
		{ "enb",	required_argument, NULL, 'b' },
		{ "range",	required_argument, NULL, 'r' },
		{ "maxspeed",	required_argument, NULL, 'm' },
		{ "pwmdiv",	required_argument, NULL, 'p' },
		{ "help",	no_argument,       NULL, 'h' },
		{ 0, 0, 0, 0 } };

	for (;;) {
		c = getopt_long(argc, argv, "w:x:y:z:a:b:r:m:p:h", opts, NULL);
		if (c < 0) break;

		switch (c) {
		case 'w': int1 = atoi(optarg); break;
		case 'x': int2 = atoi(optarg); break;
		case 'y': int3 = atoi(optarg); break;
		case 'z': int4 = atoi(optarg); break;
		case 'a': ena = atoi(optarg); break;
		case 'b': enb = atoi(optarg); break;
		case 'r': range = atoi(optarg); break;
		case 'm': max_speed = atoi(optarg); break;
		case 'p': pwm_div = atoi(optarg); break;
		case 'h':
		default: pr_usage = 1; break;
		}
	}

	if (pr_usage != 0) {
		usage();
		return 1;
	}

	if (!bcm2835_init())
		return 1;

	l298n_init();

	printf("range = %d, step = %d, max_speed = %d\n", range, step, max_speed);
	printf("w: <forward> q: <left forward> e: <right forward>\n");
	printf("s: <reverse> a: <left reverse> d: <right reverse>\n");
	printf("x: <brake>   z: <left brake>   c: <right brake>\n");
	printf("type . quit\n\n");

	do {

		printf("\r\t%02d\t%02d", left_speed, right_speed);
		inchar = getch_(0);
		switch (inchar) {
		case 'w': /* forward */
			do_forward(1);
			break;
		case 's': /* reverse */
			do_reverse(1);
			break;
		case 'x': /* brake */
			do_brake();
			break;
		case 'q': /* left forward */
			do_forward_xx(left);
			break;
		case 'e': /* right forward */
			do_forward_xx(right);
			break;
		case 'a': /* left reverse */
			do_reverse_xx(left);
			break;
		case 'd': /* right reverse */
			do_reverse_xx(right);
			break;
		case 'z': /* left brake */
			do_brake_xx(left);
			break;
		case 'c': /* right brake */
			do_brake_xx(right);
			break;
		case 65:
			do_forward(0);
			break;
		case 66:
			do_reverse(0);
			break;
		case 68:
			do_left();
			break;
		case 67:
			do_right();
			break;
		case 53:
			do_speedup();
			break;
		case 54:
			do_speeddown();
			break;
		}

	} while (inchar != '.');
	
	printf("\n");

	do_brake();

	bcm2835_close();

	return 0;
}
