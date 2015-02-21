#ifndef __PIDCTRL_H__
#define __PIDCTRL_H__

int pidctrl_init(int front, int rear, int left, int right,
                long (*get_altitude)(unsigned long *timestamp),
                float angle[], float rate[], float alti[]);
void pidctrl_exit(void);

#endif /* __PIDCTRL_H__ */
