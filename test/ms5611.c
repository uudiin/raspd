#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <errno.h>
#include <unistd.h>
#include <math.h>

#include <bcm2835.h>
#include <event2/event.h>

#include "../raspd/ms5611.h"
#include "../raspd/event.h"

struct event *timer;
struct ms5611_dev *dev;

static void cb_timer(int fd, short what, void *arg)
{
    double p = ms5611_get_pressure(dev);

    printf("pressure: %f, altitude: %f\n",
            p, ms5611_get_altitude(dev));
}

int main(int argc, char *argv[])
{

    if (!bcm2835_init())
        return 1;
    bcm2835_i2c_begin();
    bcm2835_i2c_setClockDivider(BCM2835_I2C_CLOCK_DIVIDER_626);

    rasp_event_init();

    dev = ms5611_new(1025.25);
    if (dev != NULL) {
        struct timeval timeout  = {0, 10000};

        register_timer(EV_PERSIST, &timeout, cb_timer, NULL, &timer);

        event_base_dispatch(evbase);

        ms5611_del(dev);
    }

    bcm2835_i2c_end();
    bcm2835_close();

    return 0;
}
