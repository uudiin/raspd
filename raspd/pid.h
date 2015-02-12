#ifndef __PID_H__
#define __PID_H__

struct pid_struct {
    long kp;
    long ki;
    long kd;

    long error;
    long sum_err;
    long dt_err;
    long last_input;
    long min;
    long max;
    long output;
};

struct pid_struct *pid_new(long kp, long ki, long kd, long min, long max);
void pid_del(struct pid_struct *pid);
float pid_update(struct pid_struct *pid,
        long setpoint, long input, unsigned long dt);
void pid_set(struct pid_struct *pid,
        long kp, long ki, long kd, long min, long max);
void pid_set_tunings(struct pid_struct *pid, long kp, long ki, long kd);
void pid_set_windup_bounds(struct pid_struct *pid, long min, long max);
void pid_reset(struct pid_struct *pid);

#endif /* __PID_H__ */
