#ifndef __ULTRASONIC_H__
#define __ULTRASONIC_H__

int ultrasonic_scope(int count, int interval,
            int (*urgent_cb)(double distance/* cm */, void *opaque),
            void *opaque);

unsigned int ultrasonic_is_using(void);

#endif /* __ULTRASONIC_H__ */
