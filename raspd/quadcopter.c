#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <time.h>
#include <getopt.h>
#include <math.h>

#include <inv_mpu.h>
#include <inv_mpu_dmp_motion_driver.h>
#include <ml_math_func.h>

#include "inv_imu.h"
#include "module.h"
#include "softpwm.h"
#include "luaenv.h"
#include "pid.h"

#include "quadcopter.h"

#include "config.h"

#define min(x, y) (((x) < (y)) ? (x) : (y))
#define max(x, y) (((x) > (y)) ? (x) : (y))

#define LOGE(...)   fprintf(stderr, __VA_ARGS__)
#define LOGI(...)   fprintf(stdout, __VA_ARGS__)

#define PITCH 0
#define ROLL  1
#define YAW   2

static struct pid_struct pid_euler[3];
static struct pid_struct pid_euler_rate[3];
static struct pid_struct pid_altitude;

struct esc_info {
    int pin;
    int throttle;
};
static struct esc_info esc_front;
static struct esc_info esc_rear;
static struct esc_info esc_left;
static struct esc_info esc_right;

/* TODO FIXME */
static int min_throttle = 200;
static int max_throttle = 260;

static double dst_euler[3];
static double dst_altitude;

/* TODO */
static long (*fptr_get_altitude)(unsigned long *timestamp);


#ifdef CUBE_HOSTNAME

#include <sock.h>

static int use_udp = 1;
static int fd = -1;
static union sockaddr_u addr;

#define BUF_SIZE        (256)
#define PACKET_LENGTH   (23)

#define PACKET_DEBUG    (1)
#define PACKET_QUAT     (2)
#define PACKET_DATA     (3)


void eMPL_send_quat(long *quat)
{
    char out[PACKET_LENGTH];
    ssize_t n;

    if (!quat || fd < 0)
        return;
    memset(out, 0, PACKET_LENGTH);
    out[0] = '$';
    out[1] = PACKET_QUAT;
    out[3] = (char)(quat[0] >> 24);
    out[4] = (char)(quat[0] >> 16);
    out[5] = (char)(quat[0] >> 8);
    out[6] = (char)quat[0];
    out[7] = (char)(quat[1] >> 24);
    out[8] = (char)(quat[1] >> 16);
    out[9] = (char)(quat[1] >> 8);
    out[10] = (char)quat[1];
    out[11] = (char)(quat[2] >> 24);
    out[12] = (char)(quat[2] >> 16);
    out[13] = (char)(quat[2] >> 8);
    out[14] = (char)quat[2];
    out[15] = (char)(quat[3] >> 24);
    out[16] = (char)(quat[3] >> 16);
    out[17] = (char)(quat[3] >> 8);
    out[18] = (char)quat[3];
    out[21] = '\r';
    out[22] = '\n';

    if (use_udp) {
        size_t sa_len;
#ifdef HAVE_SOCKADDR_SA_LEN
        sa_len = addr.sockaddr.sa_len;
#else
        sa_len = sizeof(addr);
#endif
        n = sendto(fd, out, PACKET_LENGTH, 0, &addr.sockaddr, sa_len);
    } else {
        n = send(fd, out, PACKET_LENGTH, 0);
    }
    if (n < 0)
        LOGE("send error, errno = %d\n", errno);
}

#endif /* ! CUBE_HOSTNAME */

static void update_pwm(void)
{
    esc_front.throttle = max(min(max_throttle, esc_front.throttle), min_throttle);
    esc_rear.throttle  = max(min(max_throttle, esc_rear.throttle),  min_throttle);
    esc_left.throttle  = max(min(max_throttle, esc_left.throttle),  min_throttle);
    esc_right.throttle = max(min(max_throttle, esc_right.throttle), min_throttle);

    softpwm_set_data(esc_front.pin, esc_front.throttle);
    softpwm_set_data(esc_rear.pin,  esc_rear.throttle);
    softpwm_set_data(esc_left.pin,  esc_left.throttle);
    softpwm_set_data(esc_right.pin, esc_right.throttle);
}

/*
 * executed period
 */
static void attitude_control(double target_euler[], double euler[],
                        short gyro_short[], long timestamp)
{
    double gyro[3];
    double dt;
    double pidout[3];

    gyro[0] = (double)(gyro_short[0] / 65536.f);
    gyro[1] = (double)(gyro_short[1] / 65536.f);
    gyro[2] = (double)(gyro_short[2] / 65536.f);
    dt = (double)(timestamp / 1000.f);

    /*
     * angle control is only done on PITCH and ROLL
     * YAW is rate PID only
     */
    pidout[YAW]   = target_euler[YAW];
#define PID(x) \
    pidout[x] = pid_update(&pid_euler[x], target_euler[x], euler[x], dt)
    PID(PITCH);
    PID(ROLL);
#undef PID

    fprintf(stdout, "EULER: %.2f %.2f %.2f -- GYRO: %.2f %.2f %.2f -- PID: %.2f %.2f %.2f\n",
            euler[0], euler[1], euler[2], gyro[0], gyro[1], gyro[2],
            pidout[PITCH], pidout[ROLL], pidout[YAW]);

    /* rate control */
#define PID(x) \
    pidout[x] = pid_update(&pid_euler_rate[x], pidout[x], gyro[x], dt)
    PID(YAW);
    PID(PITCH);
    PID(ROLL);
#undef PID

    fprintf(stdout, "\t\t\t\t\t\t\t\t\t\tPIDRATE: %.2f %.2f %.2f -- THROTTLE: %.2f %.2f %.2f %.2f\n",
            pidout[PITCH], pidout[ROLL], pidout[YAW],
            ( pidout[PITCH] + pidout[YAW]),
            (-pidout[PITCH] + pidout[YAW]),
            ( pidout[ROLL]  - pidout[YAW]),
            (-pidout[ROLL]  - pidout[YAW]));

    /* set throttle */
    esc_front.throttle += ( pidout[PITCH] + pidout[YAW]);
    esc_rear.throttle  += (-pidout[PITCH] + pidout[YAW]);
    esc_left.throttle  += ( pidout[ROLL]  - pidout[YAW]);
    esc_right.throttle += (-pidout[ROLL]  - pidout[YAW]);
    update_pwm();
}

static void altitude_control(long target, long current,
                        short accel_short[], long dt)
{
    long accel[3];
    double pidout;

    /* XXX: only Z axis */
    accel[2] = (long)accel_short[2];

    pidout = pid_update(&pid_altitude, target, current, dt);
    pidout = pid_update(&pid_altitude, pidout, accel[2], dt);

    /* set throttle */
    esc_front.throttle += pidout;   /* TODO:  / 65536 ? */
    esc_rear.throttle  += pidout;
    esc_left.throttle  += pidout;
    esc_right.throttle += pidout;
    update_pwm();
}

static void tap_cb(unsigned char direction, unsigned char count)
{
    switch (direction) {
    case TAP_X_UP:   LOGI("Tap X+ "); break;
    case TAP_X_DOWN: LOGI("Tap X- "); break;
    case TAP_Y_UP:   LOGI("Tap Y+ "); break;
    case TAP_Y_DOWN: LOGI("Tap Y- "); break;
    case TAP_Z_UP:   LOGI("Tap Z+ "); break;
    case TAP_Z_DOWN: LOGI("Tap Z- "); break;
    default: return;
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

static long q29_mult(long a, long b)
{
    long long temp;
    long result;
    temp = (long long)a * b;
    result = (long)(temp >> 29);
    return result;
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
static void quat_to_euler(const long *quat, double *data)
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
    data[0] = (double)values[0];
    data[1] = (double)values[1];
    data[2] = (double)values[2];
}

static void imu_ready_cb(short sensors, unsigned long timestamp, long quat[],
            short accel[], short gyro[], short compass[], long temperature)
{
    static unsigned long prev_timestamp;
    unsigned long dt;
    long cur_altitude = -1;

    if (prev_timestamp == 0)
        dt = 1;     /* FIXME:  ms ? */
    else
        dt = timestamp - prev_timestamp;

    if (dt == 0)
        dt = 1;

    prev_timestamp = timestamp;

    if (fptr_get_altitude)
        cur_altitude = fptr_get_altitude(NULL);

    if (cur_altitude != -1 && (sensors & INV_XYZ_ACCEL))
        altitude_control(dst_altitude, cur_altitude, accel, dt);

    if ((sensors & INV_XYZ_GYRO) && (sensors & INV_WXYZ_QUAT)) {
        double euler[3];
        quat_to_euler(quat, euler);
        attitude_control(dst_euler, euler, gyro, dt);


#ifdef CUBE_HOSTNAME
        eMPL_send_quat(quat);
#endif /* ! CUBE_HOSTNAME */

        /*
        fprintf(stderr,
            "throttle: %d %d %d %d  -- gyro: %4d %4d %4d  -- euler: %.2f %.2f %.2f\n",
            esc_front.throttle, esc_rear.throttle,
            esc_left.throttle, esc_right.throttle,
            gyro[0], gyro[1], gyro[2],
            euler[0] / 65536.f, euler[1] / 65536.f, euler[2] / 65536.f);
        */
    }
}

/* TODO */
int pidctrl_init(int front, int rear, int left, int right,
                long (*get_altitude)(unsigned long *timestamp),
                double angle[], double rate[], double alti[])
{
    int i;
    /* set Kp, Ki, Kd */
    for (i = 0; i < 3; i++)
        pid_set(&pid_euler[i], angle[0], angle[1], angle[2], angle[3], angle[4]);
    for (i = 0; i < 3; i++)
        pid_set(&pid_euler_rate[i], rate[0], rate[1], rate[2], rate[3], rate[4]);
    pid_set(&pid_altitude, alti[0], alti[1], alti[2], alti[3], alti[4]);

    /*
    for (i = 0; i < 5; i++) {
        fprintf(stdout, "angle = %ld, rate = %ld, alti = %ld\n",
                angle[i], rate[i], alti[i]);
    }
    */

    esc_front.throttle = min_throttle;
    esc_rear.throttle = min_throttle;
    esc_left.throttle = min_throttle;
    esc_right.throttle = min_throttle;

    esc_front.pin = front;
    esc_rear.pin =  rear;
    esc_left.pin =  left;
    esc_right.pin = right;

    /* which altimeter should be used */
    fptr_get_altitude = get_altitude;

    /* TODO FIXME : self-test */
    invmpu_self_test();

    invmpu_register_tap_cb(tap_cb);
    invmpu_register_android_orient_cb(android_orient_cb);
    invmpu_register_data_ready_cb(imu_ready_cb);

#ifdef CUBE_HOSTNAME
    if (CUBE_HOSTNAME && CUBE_PORT) {
        char *hostname = CUBE_HOSTNAME;
        long long_port = CUBE_PORT;
        size_t ss_len = sizeof(addr);
        int err;

        use_udp = CUBE_USE_UDP;
        err = resolve(hostname, (unsigned short)long_port,
                        &addr.storage, &ss_len, AF_INET, 0);
        if (err < 0) {
            LOGE("Error hostname\n");
            exit(1);
        }

        if (use_udp) {
            fd = socket(addr.storage.ss_family, SOCK_DGRAM, 0);
        } else {
            fd = do_connect(SOCK_STREAM, &addr);
        }

        if (fd < 0) {
            LOGE("Error connect\n");
        } else {
            unblock_socket(fd);
            LOGI("connected  %s:%ld\n", hostname, long_port);
        }
    }
#endif /* ! CUBE_HOSTNAME */
    return 0;
}

void pidctrl_exit(void)
{
    /* NOTHING */
}

/*
 * module
 */

static const char *file_cal;

static int euler_init(void)
{
    const char *file = NULL;
    int err;

    err = luaenv_getconf_str("_G", "mpu_cal", &file);
    if (err >= 0 && file) {
        file_cal = strdup(file);
        luaenv_pop(1);
    } else {
        fprintf(stderr, "luaenv_getconf_str(mpu_cal), err = %d\n", err);
    }
    return err;
}

static void euler_exit(void)
{
    if (file_cal)
        free((void *)file_cal);
}

static int euler_main(int fd, int argc, char *argv[])
{
    int yaw = 0, pitch = 0, roll = 0;
    int self_test = 0;
    static struct option options[] = {
        { "yaw",       required_argument, NULL, 'y' },
        { "pitch",     required_argument, NULL, 'p' },
        { "roll",      required_argument, NULL, 'r' },
        { "self-test", no_argument,       NULL, 's' },
        { 0, 0, 0, 0 }
    };
    int c;

    while ((c = getopt_long(argc, argv, "y:p:r:s", options, NULL)) != -1) {
        switch (c) {
        case 'y': yaw = atoi(optarg); break;
        case 'p': pitch = atoi(optarg); break;
        case 'r': roll = atoi(optarg); break;
        case 's': self_test = 1; break;
        default:
            return 1;
        }
    }

    /* self test, get calibrated data */
    if (file_cal && self_test) {
        int result;
        long gyro[3], accel[3];
        time_t t;
        FILE *fp_cal;

        result = invmpu_get_calibrate_data(gyro, accel);
        if (result == 0) {
            fp_cal = fopen(file_cal, "w+");
            if (fp_cal == NULL) {
                fprintf(stderr, "fopen(%s), error\n", file_cal);
                return 1;
            }

            LOGE("Passed!\n");
            LOGE("accel: %7.4f %7.4f %7.4f\n",
                        accel[0]/65536.f,
                        accel[1]/65536.f,
                        accel[2]/65536.f);
            LOGE("gyro: %7.4f %7.4f %7.4f\n",
                        gyro[0]/65536.f,
                        gyro[1]/65536.f,
                        gyro[2]/65536.f);

            t = time(NULL);
            fprintf(fp_cal,
                "-- automatically generated, do not edit\n"
                "-- time: %s\n"
                "\n"
                "cal_gyro  = { %ld, %ld, %ld }\n"
                "cal_accel = { %ld, %ld, %ld }\n",
                ctime(&t),
                gyro[0], gyro[1], gyro[2],
                accel[0], accel[1], accel[2]);

            fclose(fp_cal);
            return 0;
        } else {
            if (!(result & 0x1))
                LOGE("Gyro failed.\n");
            if (!(result & 0x2))
                LOGE("Accel failed.\n");
            if (!(result & 0x4))
                LOGE("Compass failed.\n");

            return 1;
        }
    }

    if (yaw)
        dst_euler[YAW]   = yaw;
    if (pitch)
        dst_euler[PITCH] = pitch;
    if (roll)
        dst_euler[ROLL]  = roll;

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
    return 0;
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
    return 0;
}

DEFINE_MODULE_INIT_EXIT(euler);
DEFINE_MODULE(altitude);
DEFINE_MODULE(throttle);
