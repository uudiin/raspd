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
    struct timeval tv_delay;
    /* dynamic */
    struct event *ev_timer;
    struct event *ev_trig_done;
    struct event *ev_delay;
    struct event *ev_echo;
    struct timeval echo_tv;
    int nr_trig;
    int nr_echo;
    int count;
    int counted;
    int flags;
#define UF_IMMEDIATE    1

    float distance;
    unsigned long timestamp;

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
float ultrasonic_get_distance(struct ultrasonic_dev *dev,
                                unsigned long *timestamp);

#endif /* __ULTRASONIC_H__ */
