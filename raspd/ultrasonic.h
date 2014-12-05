#ifndef __ULTRASONIC_H__
#define __ULTRASONIC_H__

int ultrasonic_scope(int count, int interval,
            void (*urgent_cb)(double distance/* cm */, void *opaque),
            void *opaque);

#endif /* __ULTRASONIC_H__ */
