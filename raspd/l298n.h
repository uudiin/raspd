#ifndef __L298N_H__
#define __L298N_H__

struct l298n_dev {
    int ena;
    int enb;
    int in1;
    int in2;
    int in3;
    int in4;
    int max_speed;
    int range;
    int pwm_div;

    int left_speed;
    int right_speed;
    int left_forward;
    int right_forward;
    int step;
};

struct l298n_dev *l298n_new(int ena, int enb, int in1, int in2, int in3, int in4,
                            int max_speed, int range, int pwm_div);

void l298n_del(struct l298n_dev *dev);

int l298n_lup(struct l298n_dev *dev);
int l298n_ldown(struct l298n_dev *dev);
int l298n_lbrake(struct l298n_dev *dev);
int l298n_lspeedup(struct l298n_dev *dev);
int l298n_lspeeddown(struct l298n_dev *dev);
int l298n_rup(struct l298n_dev *dev);
int l298n_rdown(struct l298n_dev *dev);
int l298n_rbrake(struct l298n_dev *dev);
int l298n_rspeedup(struct l298n_dev *dev);
int l298n_rspeeddown(struct l298n_dev *dev);

#endif /* __L298N_H__ */
