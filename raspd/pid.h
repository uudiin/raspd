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

struct pid_struct *pid_new(float kp, float ki, float kd, float min, float max);
void pid_del(struct pid_struct *pid);
float pid_update(struct pid_struct *pid,
        float setpoint, float input, float dt);
void pid_set(struct pid_struct *pid,
        float kp, float ki, float kd, float min, float max);
void pid_set_tunings(struct pid_struct *pid, float kp, float ki, float kd);
void pid_set_windup_bounds(struct pid_struct *pid, float min, float max);
void pid_reset(struct pid_struct *pid);

#endif /* __PID_H__ */
