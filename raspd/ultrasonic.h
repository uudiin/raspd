#ifndef __ULTRASONIC_H__
#define __ULTRASONIC_H__

int ultrasonic_scope(int count, int interval,
            int (*urgent_cb)(double distance/* cm */, void *opaque),
            void *opaque);

unsigned int ultrasonic_is_using(void);

/************************************************************/
struct ultrasonic_dev;

typedef int (*__cb_ultrasonic)(struct ultrasonic_dev *dev,
                                double distance, void *opaque);

struct ultrasonic_dev {
    int pin_trig;
    int pin_echo;
    int trig_time;  /* us */
    struct timeval tv_trig;
    /* dynamic */
    struct event *ev_over_trig;
    struct event *ev_echo;
    struct timespec echo_tp;
    int nr_trig;
    int nr_echo;

    __cb_ultrasonic cb;
    void *opaque;
    void *userdata;
};

struct ultrasonic_dev *ultrasonic_new(int pin_trig,
                                int pin_echo, int trig_time);
void ultrasonic_del(struct ultrasonic_dev *dev);
int ultrasonic(struct ultrasonic_dev *dev, __cb_ultrasonic cb, void *opaque);
unsigned int ultrasonic_is_busy(struct ultrasonic_dev *dev);

#endif /* __ULTRASONIC_H__ */
