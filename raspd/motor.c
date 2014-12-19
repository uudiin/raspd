#include <stdio.h>
#include <errno.h>

#include <bcm2835.h>

/*
#define DEFAULT_PIN1 XX
#define DEFAULT_PIN2 XX
#define DEFAULT_PIN3 XX
#define DEFAULT_PIN4 XX

#define PULSES_CYCLE    XXX

static int pin1 = DEFAULT_PIN1;
static int pin2 = DEFAULT_PIN2;
static int pin3 = DEFAULT_PIN3;
static int pin4 = DEFAULT_PIN4;

static unsigned int pulses_four[4];
static unsigned int pulses_dfour[4];
static unsigned int pulses_eight[8];
*/

enum {
    SMF_PULSE_FOUR
    SMF_PULSE_DFOUR
    SMF_PULSE_EIGHT
};

#define INIT_PULSE_FOUR(x, pin1, pin2, pin3, pin4)  \
    do {                                            \
        (x)[0] = 1 << (pin1);                       \
        (x)[1] = 1 << (pin2);                       \
        (x)[2] = 1 << (pin3);                       \
        (x)[3] = 1 << (pin4);                       \
    } while (0)

#define INIT_PULSE_DFOUR(x, pin1, pin2, pin3, pin4) \
    do {                                            \
        (x)[0] = 1 << (pin1) | 1 << (pin2);         \
        (x)[1] = 1 << (pin2) | 1 << (pin3);         \
        (x)[2] = 1 << (pin3) | 1 << (pin4);         \
        (x)[3] = 1 << (pin4) | 1 << (pin1);         \
    } while (0)

#define INIT_PULSE_EIGHT(x, pin1, pin2, pin3, pin4) \
    do {                                            \
        (x)[0] = 1 << (pin1);                       \
        (x)[1] = 1 << (pin1) | 1 << (pin2);         \
        (x)[2] = 1 << (pin2);                       \
        (x)[3] = 1 << (pin2) | 1 << (pin3);         \
        (x)[4] = 1 << (pin3);                       \
        (x)[5] = 1 << (pin3) | 1 << (pin4);         \
        (x)[6] = 1 << (pin4);                       \
        (x)[7] = 1 << (pin4) | 1 << (pin1);         \
    } while (0)

struct stepmotor_dev {
    int pin1;
    int pin2;
    int pin3;
    int pin4;
    unsigned int *pulse;
    int nr_pulse;
    unsigned int pulses_four[4];
    unsigned int pulses_dfour[4];
    unsigned int pulses_eight[8];
    int nr_cycle_pulse;
    int angle;
    int velocity;
    int flags;
    int stoped;
    void *userdata;
};

struct stepmotor_dev *stepmotor_new(int pin1, int pin2,
                            int pin3, int pin4, int flags)
{
    struct stepmotor_dev *dev;

    dev = xmalloc(sizeof(*dev));
    if (dev) {
        memset(dev, 0, sizeof(*dev));
        dev->pin1 = pin1;
        dev->pin2 = pin2;
        dev->pin3 = pin3;
        dev->pin4 = pin4;
        INIT_PULSE_FOUR(dev->pulses_four, pin1, pin2, pin3, pin4);
        INIT_PULSE_DFOUR(dev->pulses_dfour, pin1, pin2, pin3, pin4);
        INIT_PULSE_EIGHT(dev->pulses_eight, pin1, pin2, pin3, pin4);

        switch (flags) {
        case SMF_PULSE_FOUR:
            dev->pulse = dev->pulses_four;
            dev->nr_pulse = 4;
            break;
        case SMF_PULSE_DFOUR:
            dev->pulse = dev->pulses_dfour;
            dev->nr_pulse = 4;
            break;
        case SMF_PULSE_EIGHT:
        default:
            dev->pulse = dev->pulses_eight;
            dev->nr_pulse = 8;
            break;
        }

        dev->flags = flags;
        dev->nr_cycle_pulse = ??;
        dev->stoped = 1;
    }
    return dev;
}

void stepmotor_del(struct stepmotor_dev *dev)
{
    if (dev)
        free(dev);
}

int stepmotor(struct stepmotor_dev *dev, int angle, int velocity)
{
}
