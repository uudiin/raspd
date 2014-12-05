#ifndef __PWM_H__
#define __PWM_H__

int pwm_set(int gpio, int range, int data, int incre);

int pwm_gradual(int gpio, int count, int range,
            int init_data, int step, int interval/* ms */);

int pwm_breath(int gpio, int count, int range,
            int init_data, int step, int interval/* ms */);

#endif /* __PWM_H__ */
