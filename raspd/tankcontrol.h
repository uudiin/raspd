#ifndef __TANKCONTROL_H__
#define __TANKCONTROL_H__

#include <queue.h>

struct code_entry { 
    TAILQ_ENTRY(code_entry) link;
    int code;
    int count;
};

TAILQ_HEAD(code_qh, code_entry);

struct tank_dev {
    int pin;

    int current_code;
    int speed;
    struct event *ev;
    struct code_qh qh;

	int code;
	int step;
};

struct tank_dev *tank_new(int pin);
void tank_del(struct tank_dev *dev);

void tank_sdown(struct tank_dev *dev);
void tank_sup(struct tank_dev *dev);
void tank_brake(struct tank_dev *dev);
void tank_fwd(struct tank_dev *dev);
void tank_rev(struct tank_dev *dev);
void tank_left(struct tank_dev *dev);
void tank_right(struct tank_dev *dev);
void tank_turret_left(struct tank_dev *dev);
void tank_turret_right(struct tank_dev *dev);
void tank_turret_elev(struct tank_dev *dev);
void tank_fire(struct tank_dev *dev);

#endif /* __TANKCONTROL_H__ */
