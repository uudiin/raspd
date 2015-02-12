#ifndef __PIDCTRL_H__
#define __PIDCTRL_H__

int pidctrl_init(const char *front, const char *rear, const char *left,
                const char *right, const char *altimeter,
                float angle[], float rate[], float alt[]);
void pidctrl_exit(void);

#endif /* __PIDCTRL_H__ */
