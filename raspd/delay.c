#include <stdio.h>
#include <errno.h>

#include <bcm2835.h>

#include "module.h"

static void delay(unsigned int nanosec)
{
    struct timespec t;
#define NS_PER_S    (1000 * 1000 * 1000)
    t.tv_sec = (time_t)(nanosec / NS_PER_S);
    t.tv_nsec = (long)(nanosec % NS_PER_S);
    nanosleep(&t, NULL);
}

static int delay_main(int argc, char *argv[])
{
    static struct option options[] = {
        { "second",      no_argument, NULL, 's' },
        { "millisecond", no_argument, NULL, 'm' },
        { "microsecond", no_argument, NULL, 'u' },
        { "nanosecond",  no_argument, NULL, 'n' },
        { 0, 0, 0, 0 }
    };
}

DEFINE_MODULE(delay);
