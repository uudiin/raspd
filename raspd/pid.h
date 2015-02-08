#ifndef __PID_H__
#define __PID_H__

struct pid_struct {
    float kp;
    float ki;
    float kd;

    float error;
    float sum_err;
    float dt_err;
    float last_input;
    float min;
    float max;
    float output;
};
#endif /* __PID_H__ */
