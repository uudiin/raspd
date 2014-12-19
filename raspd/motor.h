#ifndef __MOTOR_H__
#define __MOTOR_H__

enum {
    SMF_PULSE_FOUR,
    SMF_PULSE_DFOUR,
    SMF_PULSE_EIGHT
};

struct stepmotor_dev;

typedef int (*__cb_stepmotor_done)(struct stepmotor_dev *dev, void *opaque);

struct stepmotor_dev {
    int pin1;
    int pin2;
    int pin3;
    int pin4;
    unsigned int pin_mask;
    unsigned int *pulse;
    int nr_pulse;
    unsigned int pulses_four[4];
    unsigned int pulses_dfour[4];
    unsigned int pulses_eight[8];
    int nr_cycle_pulse;
    double step_angle;
    int reduction_ratio;
    int pullin_freq;
    int pullout_freq;
    /* dynamic */
    struct event *ev;
    int angle;
    int delay; /* velocity */
    int remain_pulses;
    int pidx; /* pulse index */
    int flags;
    __cb_stepmotor_done cb;
    void *opaque;
    void *userdata;
};

struct stepmotor_dev *stepmotor_new(int pin1, int pin2, int pin3,
                        int pin4, double step_angle, int reduction_ratio,
                        int pullin_freq, int pullout_freq, int flags);

void stepmotor_del(struct stepmotor_dev *dev);

int stepmotor(struct stepmotor_dev *dev, double angle,
        int delay/* ms */, __cb_stepmotor_done cb, void *opaque);

#endif /* __MOTOR_H__ */
