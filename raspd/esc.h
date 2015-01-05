#ifndef __ESC_H__
#define __ESC_H__

#include <event2/event.h>

struct esc_dev;

typedef int (*__cb_esc_started)(struct esc_dev *dev, void *opaque);

struct esc_dev {
    int pin;
    int refresh_rate;   /* hz */
    int min_throttle_time; /* us */
    int max_throttle_time; /* us */
    int throttle_time;
    int period;         /* us */
    int start_time;     /* ms */
    struct event *ev_started;
    struct event *ev_timer;
    struct event *ev_throttle;

    __cb_esc_started cb_started;
    void *opaque;
};

struct esc_dev *esc_new(int pin, int refresh_rate, int start_time,
                        int min_throttle_time, int max_throttle_time);
void esc_del(struct esc_dev *dev);
int esc_get(struct esc_dev *dev);
int esc_set(struct esc_dev *dev, int throttle_time);
int esc_start(struct esc_dev *dev, __cb_esc_started cb_started, void *opaque);
int esc_stop(struct esc_dev *dev);

#endif /* __ESC_H__ */
