#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include "pid.h"

struct pid_struct *pid_new(double kp, double ki, double kd, double min, double max)
{
    struct pid_struct *pid;

    pid = malloc(sizeof(*pid));
    if (pid) {
        memset(pid, 0, sizeof(*pid));
        pid->kp = kp;
        pid->ki = ki;
        pid->kd = kd;
        pid->min = min;
        pid->max = max;
    }
    return pid;
}

void pid_del(struct pid_struct *pid)
{
    if (pid)
        free(pid);
}

double pid_update(struct pid_struct *pid,
        double setpoint, double input, double dt)
{
    /* compute error */
    pid->error = setpoint - input;
    /* integrating errors */
    pid->sum_err += pid->error * pid->ki * dt;

    /*
     * calculating error derivative
     * input derivative is used to avoid derivative kick
     */
    pid->dt_err = -pid->kd / dt * (input - pid->last_input);

    pid->output = pid->kp * pid->error + pid->sum_err + pid->dt_err;
    /* winds up boundaries */
    if (pid->output > pid->max) {
        pid->sum_err = 0.0;
        pid->output = pid->max;
    } else if (pid->output < pid->min) {
        pid->sum_err = 0.0;
        pid->output = pid->min;
    }
    pid->last_input = input;
    return pid->output;
}

void pid_set(struct pid_struct *pid,
        double kp, double ki, double kd, double min, double max)
{
    memset(pid, 0, sizeof(*pid));
    pid->kp = kp;
    pid->ki = ki;
    pid->kd = kd;
    pid->min = min;
    pid->max = max;
}

void pid_set_tunings(struct pid_struct *pid, double kp, double ki, double kd)
{
    pid->kp = kp;
    pid->ki = ki;
    pid->kd = kd;
}

void pid_set_windup_bounds(struct pid_struct *pid, double min, double max)
{
    pid->min = min;
    pid->max = max;
}

void pid_reset(struct pid_struct *pid)
{
    pid->sum_err = 0;
    pid->dt_err = 0;
    pid->last_input = 0;
}
