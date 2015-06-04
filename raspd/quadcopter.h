#ifndef __QUADCOPTER_H__
#define __QUADCOPTER_H__

int pidctrl_init(int front, int rear, int left, int right,
                long (*get_altitude)(unsigned long *timestamp),
                double angle[], double rate[], double alti[]);
void pidctrl_exit(void);

#endif /* __QUADCOPTER_H__ */
