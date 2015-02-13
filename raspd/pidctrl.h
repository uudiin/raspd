#ifndef __PIDCTRL_H__
#define __PIDCTRL_H__

int pidctrl_init(int front, int rear, int left, int right,
                long (*get_altitude)(unsigned long *timestamp),
                long angle[], long rate[], long alt[])
void pidctrl_exit(void);

#endif /* __PIDCTRL_H__ */
