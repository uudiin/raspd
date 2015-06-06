#ifndef __PID_H__
#define __PID_H__

struct pid_struct {
    double kp;
    double ki;
    double kd;

    double error;
    double sum_err;
    double dt_err;
    double last_input;
    double min;
    double max;
    double output;
};

struct pid_struct *pid_new(double kp, double ki, double kd, double min, double max);
void pid_del(struct pid_struct *pid);
double pid_update(struct pid_struct *pid,
        double setpoint, double input, double dt);
void pid_set(struct pid_struct *pid,
        double kp, double ki, double kd, double min, double max);
void pid_set_tunings(struct pid_struct *pid, double kp, double ki, double kd);
void pid_set_windup_bounds(struct pid_struct *pid, double min, double max);
void pid_reset(struct pid_struct *pid);

#endif /* __PID_H__ */
