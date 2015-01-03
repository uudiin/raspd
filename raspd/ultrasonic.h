#ifndef __ULTRASONIC_H__
#define __ULTRASONIC_H__

#include <time.h>
#include <event2/event.h>

int ultrasonic_scope0(int count, int interval,
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
    struct event *ev_timer;
    struct event *ev_over_trig;
    struct event *ev_echo;
    struct timespec echo_tp;
    int nr_trig;
    int nr_echo;
    int count;
    int counted;
    int flags;
#define UF_IMMEDIATE    1

    __cb_ultrasonic cb;
    void *opaque;
    void *userdata;
};

struct ultrasonic_dev *ultrasonic_new(int pin_trig,
                                int pin_echo, int trig_time);
void ultrasonic_del(struct ultrasonic_dev *dev);
int ultrasonic(struct ultrasonic_dev *dev, __cb_ultrasonic cb, void *opaque);
int ultrasonic_scope(struct ultrasonic_dev *dev, int count,
                int interval, __cb_ultrasonic cb, void *opaque);
unsigned int ultrasonic_is_busy(struct ultrasonic_dev *dev);

#endif /* __ULTRASONIC_H__ */
