#include <stdio.h>
#include <errno.h>

#include <bcm2835.h>

#include "module.h"

/*
 * 1s = 1000,000,000 ns (0x3b9cca00)
 * unsigned long should overflowed about more than 4s
 */
static void delay_nanosec(unsigned long nanosec)
{
    struct timespec t;
#define NS_PER_S    (1000 * 1000 * 1000)
    t.tv_sec = (time_t)(nanosec / NS_PER_S);
    t.tv_nsec = (long)(nanosec % NS_PER_S);
    nanosleep(&t, NULL);
}

void delay_millisec(unsigned long millisec)
{
    struct timespec t;
    t.tv_sec = (time_t)(millisec / 1000);
    t.tv_nsec = (long)(millisec % 1000) * 1000000;
    nanosleep(&t, NULL);
}

static int delay_main(int argc, char *argv[])
{
    static struct option options[] = {
        { "second",   required_argument, NULL, 's' },
        { "millisec", requirno_argument, NULL, 'm' },
        { "microsec", requirno_argument, NULL, 'u' },
        { "nanosec",  requirno_argument, NULL, 'n' },
        { 0, 0, 0, 0 }
    };
    int c;
    unsigned long nanosec = 0;
    unsigned long millisec = 0;

    while ((c = getopt_long(argc, argv, "s:m:u:n:", options, NULL)) != -1) {
        switch (c) {
        case 's': millisec = (unsigned long)(atoi(optarg) * 1000); break;
        case 'm': millisec = (unsigned long)atoi(optarg); break;
        case 'u': nanosec =  (unsigned long)(atoi(optarg) * 1000); break;
        case 'n': nanosec =  (unsigned long)atoi(optarg); break;
        default:
            return 1;
        }
    }

    if (optind < argc)
        millisec = atoi(argv[optind]);

    if (millisec)
        delay_millisec(millisec);
    if (nanosec)
        delay_nanosec(nanosec);

    return 0;
}

DEFINE_MODULE(delay);
