#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include "module.h"
#include "softpwm.h"
#include "inv_mpu.h"
#include "pid.h"
#include "pidctrl.h"

#define YAW   0
#define PITCH 1
#define ROLL  2

static struct pid_struct euler[3];
static struct pid_struct euler_rate[3];
static struct pid_struct altitude;

struct esc_info {
    int pin;
    int throttle;
};
static struct esc_info esc_front;
static struct esc_info esc_rear;
static struct esc_info esc_left;
static struct esc_info esc_right;

volatile static long dst_euler[3];
volatile static long dst_altitude;

static void *altimeter_dev;

/* proto */
long get_distance(void *dev);

static void update_pwm(void)
{
    softpwm_set_data(esc_front.pin, esc_front.throttle);
    softpwm_set_data(esc_rear.pin, esc_rear.throttle);
    softpwm_set_data(esc_left.pin, esc_left.throttle);
    softpwm_set_data(esc_right.pin, esc_right.throttle);
}

/*
 * executed period
 */
static void attitude_control(long target_euler[], long euler[],
                        short gyro_short[], unsigned long dt)
{
    long gyro[3];
    long pidout[3];

    gyro[0] = (long)gyro_short[0];
    gyro[1] = (long)gyro_short[1];
    gyro[2] = (long)gyro_short[2];

    /*
     * angle control is only done on PITCH and ROLL
     * YAW is rate PID only
     */
    pidout[YAW]   = target_euler[YAW];
#define PID(x) \
    pidout[x] = pid_update(&euler[x], target_euler[x], euler[x], dt)
    PID(PITCH);
    PID(ROLL);
#undef PID

    /* rate control */
#define PID(x) \
    pidout[x] = pid_update(&euler_rate[x], pidout[x], gyro[x], dt)
    PID(YAW);
    PID(PITCH);
    PID(ROLL);
#undef PID

    /* set throttle */
    esc_front.throttle += ( pidout[PITCH] + pidout[YAW]);
    esc_rear.throttle  += (-pidout[PITCH] + pidout[YAW]);
    esc_left.throttle  += ( pidout[ROLL]  - pidout[YAW]);
    esc_right.throttle += (-pidout[ROLL]  - pidout[YAW]);
    update_pwm();
}

static void altitude_control(long target, long current,
                        short accel_short[], unsigned long dt)
{
    long accel[3];
    long pidout;

    /* XXX: only Z axis */
    accel[2] = (long)accel_short[2];

    pidout = pid_update(&altitude, target, current, dt);
    pidout = pid_update(&altitude, pidout, accel[2], dt);

    /* set throttle */
    esc_front.throttle += pidout;   /* TODO:  / 65536 ? */
    esc_rear.throttle  += pidout;
    esc_left.throttle  += pidout;
    esc_right.throttle += pidout;
    update_pwm();
}

#define LOGE(...)   fprintf(stderr, __VA_ARGS__)
#define LOGI(...)   fprintf(stdout, __VA_ARGS__)

static void tap_cb(unsigned char direction, unsigned char count)
{
    switch (direction) {
    case TAP_X_UP:
        LOGI("Tap X+ ");
        break;
    case TAP_X_DOWN:
        LOGI("Tap X- ");
        break;
    case TAP_Y_UP:
        LOGI("Tap Y+ ");
        break;
    case TAP_Y_DOWN:
        LOGI("Tap Y- ");
        break;
    case TAP_Z_UP:
        LOGI("Tap Z+ ");
        break;
    case TAP_Z_DOWN:
        LOGI("Tap Z- ");
        break;
    default:
        return;
    }
    LOGI("x%d\n", count);
    return;
}

static void android_orient_cb(unsigned char orientation)
{
	switch (orientation) {
	case ANDROID_ORIENT_PORTRAIT:
        LOGI("Portrait\n");
        break;
	case ANDROID_ORIENT_LANDSCAPE:
        LOGI("Landscape\n");
        break;
	case ANDROID_ORIENT_REVERSE_PORTRAIT:
        LOGI("Reverse Portrait\n");
        break;
	case ANDROID_ORIENT_REVERSE_LANDSCAPE:
        LOGI("Reverse Landscape\n");
        break;
	default:
		return;
	}
}

/**
 *  @brief      Body-to-world frame euler angles.
 *  The euler angles are output with the following convention:
 *  Pitch: -180 to 180
 *  Roll: -90 to 90
 *  Yaw: -180 to 180
 *  @param[out] data        Euler angles in degrees, q16 fixed point.
 *  @return     1 if data was updated.
 */
void quat_to_euler(const long *quat, long *data)
{
    long t1, t2, t3;
    long q00, q01, q02, q03, q11, q12, q13, q22, q23, q33;
    float values[3];

    q00 = q29_mult(quat[0], quat[0]);
    q01 = q29_mult(quat[0], quat[1]);
    q02 = q29_mult(quat[0], quat[2]);
    q03 = q29_mult(quat[0], quat[3]);
    q11 = q29_mult(quat[1], quat[1]);
    q12 = q29_mult(quat[1], quat[2]);
    q13 = q29_mult(quat[1], quat[3]);
    q22 = q29_mult(quat[2], quat[2]);
    q23 = q29_mult(quat[2], quat[3]);
    q33 = q29_mult(quat[3], quat[3]);

    /* X component of the Ybody axis in World frame */
    t1 = q12 - q03;

    /* Y component of the Ybody axis in World frame */
    t2 = q22 + q00 - (1L << 30);
    values[2] = -atan2f((float) t1, (float) t2) * 180.f / (float) M_PI;

    /* Z component of the Ybody axis in World frame */
    t3 = q23 + q01;
    values[0] =
        atan2f((float) t3,
                sqrtf((float) t1 * t1 +
                      (float) t2 * t2)) * 180.f / (float) M_PI;
    /* Z component of the Zbody axis in World frame */
    t2 = q33 + q00 - (1L << 30);
    if (t2 < 0) {
        if (values[0] >= 0)
            values[0] = 180.f - values[0];
        else
            values[0] = -180.f - values[0];
    }

    /* X component of the Xbody axis in World frame */
    t1 = q11 + q00 - (1L << 30);
    /* Y component of the Xbody axis in World frame */
    t2 = q12 + q03;
    /* Z component of the Xbody axis in World frame */
    t3 = q13 - q02;

    values[1] =
        (atan2f((float)(q33 + q00 - (1L << 30)), (float)(q13 - q02)) *
          180.f / (float) M_PI - 90);
    if (values[1] >= 90)
        values[1] = 180 - values[1];

    if (values[1] < -90)
        values[1] = -180 - values[1];
    data[0] = (long)(values[0] * 65536.f);
    data[1] = (long)(values[1] * 65536.f);
    data[2] = (long)(values[2] * 65536.f);
}

static void imu_ready_cb(short sensors, unsigned long timestamp, long quat[],
            short accel[], short gyro[], short compass[], long temperature)
{
    static unsigned long prev_timestamp;
    unsigned long dt;
    long cur_altitude = 0;

    if (prev_timestamp == 0)
        dt = 1;     /* FIXME:  ms ? */
    else
        dt = timestamp - prev_timestamp;
    prev_timestamp = timestamp;

    if (altimeter_dev)
        cur_altitude = get_distance(altimeter_dev);

    if (cur_altitude && (sensors & INV_XYZ_ACCEL))
        altitude_control(dst_altitude, cur_altitude, accel, dt);

    if ((sensors & INV_XYZ_GYRO) && (sensors & INV_WXYZ_QUAT)) {
        long euler[3];
        quat_to_euler(quat, euler);
        attitude_control(dst_euler, euler, gyro, dt);
    }
}

int pidctrl_init(const char *front, const char *rear, const char *left,
                const char *right, const char *altimeter,
                float angle[], float rate[], float alt[])
{
    int i;
    /* set Kp, Ki, Kd */
    for (i = 0; i < 3; i++)
        pid_set(&euler[i], angle[0], angle[1], angle[2], angle[3], angle[4]);
    for (i = 0; i < 3; i++)
        pid_set(&euler_rate[i], rate[0], rate[1], rate[2], rate[3], rate[4]);
    pid_set(&altitude, alt[0], alt[1], alt[2], alt[3], alt[4]);

    esc_front.pin = (int)(intptr_t)luaenv_getdev(front);
    esc_rear.pin =  (int)(intptr_t)luaenv_getdev(rear);
    esc_left.pin =  (int)(intptr_t)luaenv_getdev(left);
    esc_right.pin = (int)(intptr_t)luaenv_getdev(right);

    invmpu_register_tap_cb(tap_cb);
    invmpu_register_android_orient_cb(android_orient_cb);
    invmpu_register_data_ready_cb(imu_ready_cb);
}

void pidctrl_exit(void)
{
    /* NOTHING */
}

/*
 * module
 */

static int euler_main(int fd, int argc, char *argv[])
{
    int yaw, pitch, roll;
    static struct option options[] = {
        { "yaw",   required_argument, NULL, 'y' },
        { "pitch", required_argument, NULL, 'p' },
        { "roll",  required_argument, NULL, 'r' },
        { 0, 0, 0, 0 }
    };
    int c;

    while ((c = getopt_long(argc, argv, "y:p:r:", options, NULL)) != -1) {
        switch (c) {
        case 'y': yaw = atoi(optarg); break;
        case 'p': pitch = atoi(optarg); break;
        case 'r': roll = atoi(optarg); break;
        default:
            return 1;
        }
    }

    dst_euler[YAW]   = (long)yaw;
    dst_euler[PITCH] = (long)pitch;
    dst_euler[ROLL]  = (long)roll;

    return 0;
}

static int altitude_main(int fd, int argc, char *argv[])
{
    int temp;
    if (argc < 2)
        return 1;
    temp = atoi(argv[1]);
    if (temp < 0)
        temp = 0;

    dst_altitude = (long)temp;
}

static int throttle_main(int fd, int argc, char *argv[])
{
    int temp;
    if (argc < 2)
        return 1;
    temp = atoi(argv[1]);

    /* FIXME: use percent */

    /* set throttle */
    esc_front.throttle += temp;
    esc_rear.throttle  += temp;
    esc_left.throttle  += temp;
    esc_right.throttle += temp;
    update_pwm();
}

DEFINE_MODULE(euler);
DEFINE_MODULE(altitude);
DEFINE_MODULE(throttle);
