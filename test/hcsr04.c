#include <stdio.h>
#include <assert.h>
#include <sys/time.h>
#include <pthread.h>

#include <bcm2835.h>

#define ECHO	23
#define TRIG	18

static void *signal_thread(void *arg)
{
	int err;
	struct timeval tv;
	unsigned long long start, stop;
	float dis;

	err = bcm2835_gpio_poll(ECHO, edge_both, -1, NULL);
	assert(err >= 0);
	gettimeofday(&tv, NULL);
	start = tv.tv_sec * 1000000 + tv.tv_usec;
	printf("start = %lu\n", start);
	
	err = bcm2835_gpio_poll(ECHO, edge_both, -1, NULL);
	assert(err >= 0);
	gettimeofday(&tv, NULL);
	stop = tv.tv_sec * 1000000 + tv.tv_usec;
	printf("stop = %lu\n", stop);

	dis = (float)(stop - start) * ((float)34000 / 1000000) / 2;
	printf("distance = %0.2f cm\n", dis);

	return NULL;
}

int main(int argc, char *argv[])
{
	pthread_t tid;
	int err;

	if (bcm2835_init() < 0)
		return 1;

	err = pthread_create(&tid, NULL, signal_thread, NULL);
	if (err != 0) {
        	return -1;
    	}

	bcm2835_delay(1000);
	bcm2835_gpio_fsel(TRIG, BCM2835_GPIO_FSEL_OUTP);
	bcm2835_gpio_write(TRIG, LOW);
	bcm2835_delay(2);
	bcm2835_gpio_write(TRIG, HIGH);
	bcm2835_delay(20);
	bcm2835_gpio_write(TRIG, LOW);

	bcm2835_delay(1000);

	bcm2835_close();

	return 0;
}
