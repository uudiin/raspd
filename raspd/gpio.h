#ifndef __GPIO_H__
#define __GPIO_H__

int gpio_blink_multi(int gpio[], int nr, int count, int interval);
int gpio_blink(int gpio, int count, int interval);

#endif /* __GPIO_H__ */
