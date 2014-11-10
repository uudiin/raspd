#include <stdio.h>
#include <errno.h>

#include <bcm2835.h>

#include "module.h"

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
