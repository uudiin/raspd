#ifndef __SOFTPWM_H__
#define __SOFTPWM_H__

int softpwm_init(int cycle_time, int step_time);
void softpwm_exit(void);
void softpwm_stop(void);
int softpwm_set_data(int pin, int data);
int softpwm_set_multi(unsigned long pinmask, int data);

#endif /* __SOFTPWM_H__ */
