#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <errno.h>
#include <getopt.h>

#include <bcm2835.h>
#include <xmalloc.h>

#include "module.h"
#include "event.h"

#include "pwm.h"

#define min(a, b)   (((a) < (b)) ? (a) : (b))
#define max(a, b)   (((a) > (b)) ? (a) : (b))

#define NR_CHANNDEL     2
#define NR_PWM_IO       4

struct pwm_channel {
    int range;
    int data;
    int initialized;
};

struct pwm_io_info {
    uint8_t gpio;
    uint8_t channel;
    uint8_t alt_fun;
    uint8_t j8_pin;
    int initialized;
};

static struct pwm_channel channels[NR_CHANNDEL];

static struct pwm_io_info pwm_ios[NR_PWM_IO] = {
    { 12, 0, BCM2835_GPIO_FSEL_ALT0, RPI_BPLUS_GPIO_J8_32 },
    { 13, 1, BCM2835_GPIO_FSEL_ALT0, RPI_BPLUS_GPIO_J8_33 },
    { 18, 0, BCM2835_GPIO_FSEL_ALT5, RPI_BPLUS_GPIO_J8_12 },
    { 19, 1, BCM2835_GPIO_FSEL_ALT5, RPI_BPLUS_GPIO_J8_35 },
};

static uint32_t clock;
static uint32_t step;

/*
 * increment:
 *      0    : set data
 *      else : set incre
 */
int pwm_set(int gpio, int range, int data, int incre)
{
    int v, n;
    int i;

    for (i = 0; i < NR_PWM_IO; i++) {
        if (gpio != pwm_ios[i].gpio)
            continue;

        v = 0;
        n = pwm_ios[i].channel;

        if (!pwm_ios[i].initialized) {
            pwm_ios[i].initialized = 1;
            bcm2835_gpio_fsel(gpio, pwm_ios[i].alt_fun);
        }

        if (!channels[n].initialized) {
            channels[n].initialized = 1;
            bcm2835_pwm_set_mode(n, 1, 1);
        }
        if (channels[n].range != range) {
            bcm2835_pwm_set_range(n, range);
            channels[n].range = range;
        }

        if (incre == 0) {
            v = min(data, channels[n].range);
            v = max(v, 0);
        } else if (incre > 0 /* up */) {
            v = min(channels[n].data + data, channels[n].range);
        } else if (incre < 0 /* down */) {
            v = max(channels[n].data - data, 0);
        }

        bcm2835_pwm_set_data(n, v);
        channels[n].data = v;

        return 0;
    }

    return -EINVAL;
}

struct pwm_gradual {
    int gpio;
    int flags;
#define PF_BREATH  1
    int count;
    int range;
    int init_data;
    int step;
    int interval;
    struct timeval timeout;
    struct event *timer;
    int counted;
    int data;
};

static void cb_timer(int fd, short what, void *arg)
{
    struct pwm_gradual *gp = arg;

    if (gp->counted++ >= gp->count) {
        eventfd_del(gp->timer);
        free(gp);
        return;
    }

    gp->data += gp->step;
    if (gp->flags & PF_BREATH) {
        if ((gp->step > 0 && gp->data > gp->range)
                || (gp->step < 0 && gp->data < 0))
            gp->step = -gp->step;
    } else if (gp->data > gp->range) {
        gp->data = gp->range;
    } else if (gp->data < 0) {
        gp->data = 0;
    }
    pwm_set(gp->gpio, gp->range, gp->data, 0);
}

static int
__pwm_gradual(int gpio, int flags, int count, int range,
                int init_data, int step, int interval/* ms */)
{
    struct pwm_gradual *gp;
    int err;

    if ((err = pwm_set(gpio, range, init_data, 0)) < 0)
        return err;

    gp = xmalloc(sizeof(*gp));
    memset(gp, 0, sizeof(*gp));
    gp->gpio = gpio;
    gp->flags = flags;
    gp->range = range;
    gp->init_data = init_data;
    gp->data = init_data;
    gp->step = step;
    gp->interval = interval;
    gp->timeout.tv_sec = interval / 1000;
    gp->timeout.tv_usec = (interval % 1000) * 1000;

    if ((err = register_timer(EV_PERSIST, &gp->timeout,
                            cb_timer, gp, &gp->timer)) < 0) {
        free(gp);
        return err;
    }
    return 0;
}

int pwm_gradual(int gpio, int count, int range,
            int init_data, int step, int interval/* ms */)
{
    return __pwm_gradual(gpio, 0, count,
                    range, init_data, step, interval);
}

int pwm_breath(int gpio, int count, int range,
            int init_data, int step, int interval/* ms */)
{
    return __pwm_gradual(gpio, PF_BREATH, count,
                    range, init_data, step, interval);
}

static pwm_main(int wfd, int argc, char *argv[])
{
    static struct option options[] = {
        { "range", required_argument, NULL, 'r' },
        { "step",  required_argument, NULL, 't' },
        { "set",   required_argument, NULL, 's' },
        { "up",    no_argument,       NULL, 'u' },
        { "down",  no_argument,       NULL, 'd' },
        { 0, 0, 0, 0 }
    };
    int c;
    uint32_t range = 1000;
    uint32_t _step = 100;
    int data = 0;
    int incre = 0;

    while ((c = getopt_long(argc, argv, "r:t:s:ud", options, NULL)) != -1) {
        switch (c) {
        case 'r': range = (uint32_t)atoi(optarg); break;
        case 't': _step = (uint32_t)atoi(optarg); break;
        case 's': data = atoi(optarg); break;
        case 'u': incre = 1; break;
        case 'd': incre = -1; break;
        default:
            return 1;
        }
    }

    if (step != _step)
        step = _step;

    while (optind < argc) {
        unsigned int io = atoi(argv[optind]);

        /* TODO  if error */
        if (incre == 0)
            pwm_set(io, range, data, 0);
        else
            pwm_set(io, range, step, incre);

        optind++;
    }

    return 0;
}

static int pwm_early_init(void)
{
    clock = BCM2835_PWM_CLOCK_DIVIDER_16;
    bcm2835_pwm_set_clock(clock);
    return 0;
}

/*
 * DEFINE_MODULE(pwm);
 */
static struct module __module_pwm = {
    .name = "pwm",
    .early_init = pwm_early_init,
    .main = pwm_main
};

static __init void __reg_module_pwm(void)
{
    register_module(&__module_pwm);
}
