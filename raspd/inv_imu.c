#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <event2/event.h>

#include <inv_mpu.h>
#include <inv_mpu_dmp_motion_driver.h>

#include <bcm2835.h>

#include "event.h"
#include "gpiolib.h"

#include "inv_imu.h"

#include "../lib/sock.h"

/* Private typedef -----------------------------------------------------------*/
#undef LOGE
#undef LOGI
#define LOGE(...)   fprintf(stderr, __VA_ARGS__)
#define LOGI(...)   fprintf(stdout, __VA_ARGS__)

/* Data read from MPL. */
#define PRINT_PEDO      (0x80)

#define ACCEL_ON        (0x01)
#define GYRO_ON         (0x02)
#define COMPASS_ON      (0x04)

/* Starting sampling rate. */
#define DEFAULT_MPU_HZ  (20)

#define PEDO_READ_MS    (1000)
#define TEMP_READ_MS    (500)
#define COMPASS_READ_MS (100)

struct hal_s {
    unsigned char lp_accel_mode;
    unsigned char sensors;
    unsigned char dmp_on;
    volatile unsigned char new_gyro;
    unsigned char motion_int_mode;
    unsigned long next_pedo_ms;
    unsigned long next_temp_ms;
    unsigned long next_compass_ms;
    unsigned int report;
    unsigned short dmp_features;

    short accel_short[3];
    short gyro[3];
    long quat[4];
    long temperature;
    short compass_short[3];
#ifdef COMPASS_ENABLED
#endif
    void (*tap_cb)(unsigned char count, unsigned char direction);
    void (*android_orient_cb)(unsigned char orientation);
    __invmpu_data_ready_cb data_ready_cb;
    struct event *ev_int;    /* event for pin interrupt */
};
static struct hal_s hal = {0};

/* Platform-specific information. Kinda like a boardfile. */
struct platform_data_s {
    signed char orientation[9];
};

/* The sensors can be mounted onto the board in any orientation. The mounting
 * matrix seen below tells the MPL how to rotate the raw data from the
 * driver(s).
 * TODO: The following matrices refer to the configuration on internal test
 * boards at Invensense. If needed, please modify the matrices to match the
 * chip-to-body matrix for your particular set up.
 */
static struct platform_data_s gyro_pdata = {
    .orientation = { 1, 0, 0,
                     0, 1, 0,
                     0, 0, 1}
};

#if defined MPU9150 || defined MPU9250
static struct platform_data_s compass_pdata = {
    .orientation = { 0, 1, 0,
                     1, 0, 0,
                     0, 0, -1}
};
#define COMPASS_ENABLED 1
#elif defined AK8975_SECONDARY
static struct platform_data_s compass_pdata = {
    .orientation = {-1, 0, 0,
                     0, 1, 0,
                     0, 0,-1}
};
#define COMPASS_ENABLED 1
#elif defined AK8963_SECONDARY
static struct platform_data_s compass_pdata = {
    .orientation = {-1, 0, 0,
                     0,-1, 0,
                     0, 0, 1}
};
#define COMPASS_ENABLED 1
#endif

/* Private function prototypes -----------------------------------------------*/
static int get_clock_ms(unsigned long *count)
{
    struct timeval tv;
    if (!count)
        return -1;
    gettimeofday(&tv, NULL);
    *count = tv.tv_sec * 1000 + tv.tv_usec / 1000;
    return 0;
}
/* ---------------------------------------------------------------------------*/

#define PACKET_LENGTH   (23)

#define PACKET_DEBUG    (1)
#define PACKET_QUAT     (2)
#define PACKET_DATA     (3)

static int fd = -1;

static void eMPL_init_connect(char *hostname, long long_port)
{
    int err;
    if (hostname && long_port) {
        union sockaddr_u addr;
        size_t ss_len = sizeof(addr);

        err = resolve(hostname, (unsigned short)long_port,
                        &addr.storage, &ss_len, AF_INET, 0);
        if (err < 0) {
            LOGE("Error hostname\n");
            return;
        }

        if ((fd = do_connect(SOCK_STREAM, &addr)) < 0) {
            LOGE("Error connect\n");
        } else {
            unblock_socket(fd);
            LOGI("connected  %s:%ld\n", hostname, long_port);
        }
    }
}

static void eMPL_deinit_connect(void)
{
    // TODO: cleanup
}

static void eMPL_send_quat(long *quat)
{
    char out[PACKET_LENGTH];

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

    if (send(fd, out, PACKET_LENGTH, 0) == -1)
        LOGE("send error, errno = %d\n", errno);
}


/* Handle sensor on/off combinations. */
static void setup_sensors(unsigned char sensors)
{
    unsigned char mask = 0, lp_accel_was_on = 0;
    if (sensors & ACCEL_ON)
        mask |= INV_XYZ_ACCEL;
    if (sensors & GYRO_ON) {
        mask |= INV_XYZ_GYRO;
        lp_accel_was_on |= hal.lp_accel_mode;
    }
#ifdef COMPASS_ENABLED
    if (sensors & COMPASS_ON) {
        mask |= INV_XYZ_COMPASS;
        lp_accel_was_on |= hal.lp_accel_mode;
    }
#endif
    /* If you need a power transition, this function should be called with a
     * mask of the sensors still enabled. The driver turns off any sensors
     * excluded from this mask.
     */
    mpu_set_sensors(mask);
    mpu_configure_fifo(mask);
    if (lp_accel_was_on)
        hal.lp_accel_mode = 0;
}

static void tap_cb(unsigned char direction, unsigned char count)
{
    if (hal.tap_cb)
        hal.tap_cb(direction, count);
}

static void android_orient_cb(unsigned char orientation)
{
    if (hal.android_orient_cb)
        hal.android_orient_cb(orientation);
}


void invmpu_self_test(void)
{
    int result;
    long gyro[3], accel[3];

#if defined (MPU6500) || defined (MPU9250)
    result = mpu_run_6500_self_test(gyro, accel, 0);
#elif defined (MPU6050) || defined (MPU9150)
    result = mpu_run_self_test(gyro, accel);
#endif
    if (result == 0x7) {
        LOGI("Passed!\n");
        LOGI("accel: %7.4f %7.4f %7.4f\n",
                    accel[0]/65536.f,
                    accel[1]/65536.f,
                    accel[2]/65536.f);
        LOGI("gyro: %7.4f %7.4f %7.4f\n",
                    gyro[0]/65536.f,
                    gyro[1]/65536.f,
                    gyro[2]/65536.f);
        /* Test passed. We can trust the gyro data here, so now we need to update calibrated data*/

#ifdef USE_CAL_HW_REGISTERS
        /*
         * This portion of the code uses the HW offset registers that are in the MPUxxxx devices
         * instead of pushing the cal data to the MPL software library
         */
        unsigned char i = 0;

        for(i = 0; i < 3; i++) {
        	gyro[i] = (long)(gyro[i] * 32.8f); /* convert to +-1000dps */
        	accel[i] *= 4096.f;                /* convert to +-8G */
        	accel[i] = accel[i] >> 16;
        	gyro[i] = (long)(gyro[i] >> 16);
        }

        mpu_set_gyro_bias_reg(gyro);

#if defined (MPU6500) || defined (MPU9250)
        mpu_set_accel_bias_6500_reg(accel);
#elif defined (MPU6050) || defined (MPU9150)
        mpu_set_accel_bias_6050_reg(accel);
#endif
#endif
    } else {
        if (!(result & 0x1))
            LOGE("Gyro failed.\n");
        if (!(result & 0x2))
            LOGE("Accel failed.\n");
        if (!(result & 0x4))
            LOGE("Compass failed.\n");
    }
}

int invmpu_get_calibrate_data(long gyro[], long accel[])
{
    int result;

#if defined (MPU6500) || defined (MPU9250)
    result = mpu_run_6500_self_test(gyro, accel, 0);
#elif defined (MPU6050) || defined (MPU9150)
    result = mpu_run_self_test(gyro, accel);
#endif
    if (result == 0x7) {
        /* Test passed. We can trust the gyro data here, so now we need to update calibrated data*/
        /*
         * This portion of the code uses the HW offset registers that are in the MPUxxxx devices
         * instead of pushing the cal data to the MPL software library
         */
        unsigned char i = 0;

        for(i = 0; i < 3; i++) {
        	gyro[i] = (long)(gyro[i] * 32.8f); /* convert to +-1000dps */
        	accel[i] *= 4096.f;                /* convert to +-8G */
        	accel[i] = accel[i] >> 16;
        	gyro[i] = (long)(gyro[i] >> 16);
        }
        return 0;
    } else {
        return result;
    }
}

void invmpu_set_calibrate_data(long gyro[], long accel[])
{
    mpu_set_gyro_bias_reg(gyro);

#if defined (MPU6500) || defined (MPU9250)
    mpu_set_accel_bias_6500_reg(accel);
#elif defined (MPU6050) || defined (MPU9150)
    mpu_set_accel_bias_6050_reg(accel);
#endif
}

static void handle_input(char c)
{
    switch (c) {
	case ',':
        /* Set hardware to interrupt on gesture event only. This feature is
         * useful for keeping the MCU asleep until the DMP detects as a tap or
         * orientation event.
         */
        dmp_set_interrupt_mode(DMP_INT_GESTURE);
        break;
    case '.':
        /* Set hardware to interrupt periodically. */
        dmp_set_interrupt_mode(DMP_INT_CONTINUOUS);
        break;
    case '6':
        /* Toggle pedometer display. */
        hal.report ^= PRINT_PEDO;
        break;
    case '7':
        /* Reset pedometer. */
        dmp_set_pedometer_step_count(0);
        dmp_set_pedometer_walk_time(0);
        break;
    case 'm':
        /* Test the motion interrupt hardware feature. */
#ifndef MPU6050 /* not enabled for 6050 product */
		hal.motion_int_mode = 1;
#endif 
        break;
    }
}

static void set_lp_quat(int lp_quat_on)
{
    /* Toggle LP quaternion.
     * The DMP features can be enabled/disabled at runtime. Use this same
     * approach for other features.
     */
    if (lp_quat_on)
        hal.dmp_features |= DMP_FEATURE_6X_LP_QUAT;
    else
        hal.dmp_features &= ~DMP_FEATURE_6X_LP_QUAT;
    dmp_enable_feature(hal.dmp_features);

    if (!(hal.dmp_features & DMP_FEATURE_6X_LP_QUAT))
        LOGI("LP quaternion disabled.\n");
    else
        LOGI("LP quaternion enabled.\n");
}

/* Set low-power accel mode. */
static void set_lp_accel(void)
{
    if (hal.dmp_on)
        /* LP accel is not compatible with the DMP. */
        return;
    mpu_lp_accel_mode(20);
    /* When LP accel mode is enabled, the driver automatically configures
     * the hardware for latched interrupts. However, the MCU sometimes
     * misses the rising/falling edge, and the hal.new_gyro flag is never
     * set. To avoid getting locked in this state, we're overriding the
     * driver's configuration and sticking to unlatched interrupt mode.
     *
     * TODO: The MCU supports level-triggered interrupts.
     */
    mpu_set_int_latched(0);
    hal.sensors &= ~(GYRO_ON|COMPASS_ON);
    hal.sensors |= ACCEL_ON;
    hal.lp_accel_mode = 1;
}

void invmpu_set_dmp_state(int dmp_on)
{
    if (hal.lp_accel_mode)
        /* LP accel is not compatible with the DMP. */
        return;
    /* Toggle DMP. */
    if (dmp_on) {
        unsigned short dmp_rate;
        unsigned char mask = 0;
        hal.dmp_on = 0;
        mpu_set_dmp_state(0);
        /* Restore FIFO settings. */
        if (hal.sensors & ACCEL_ON)
            mask |= INV_XYZ_ACCEL;
        if (hal.sensors & GYRO_ON)
            mask |= INV_XYZ_GYRO;
        if (hal.sensors & COMPASS_ON)
            mask |= INV_XYZ_COMPASS;
        mpu_configure_fifo(mask);
        /* When the DMP is used, the hardware sampling rate is fixed at
         * 200Hz, and the DMP is configured to downsample the FIFO output
         * using the function dmp_set_fifo_rate. However, when the DMP is
         * turned off, the sampling rate remains at 200Hz. This could be
         * handled in inv_mpu.c, but it would need to know that
         * inv_mpu_dmp_motion_driver.c exists. To avoid this, we'll just
         * put the extra logic in the application layer.
         */
        dmp_get_fifo_rate(&dmp_rate);
        mpu_set_sample_rate(dmp_rate);
        LOGI("DMP disabled.\n");
    } else {
        unsigned short sample_rate;
        hal.dmp_on = 1;
        /* Preserve current FIFO rate. */
        mpu_get_sample_rate(&sample_rate);
        dmp_set_fifo_rate(sample_rate);
        mpu_set_dmp_state(1);
        LOGI("DMP enabled.\n");
    }
}

/*
 * The compass rate is never changed.
 */
void invmpu_set_sample_rate(int rate)
{
    if (hal.dmp_on)
        dmp_set_fifo_rate(rate);
    else
        mpu_set_sample_rate(rate);
}

void invmpu_register_tap_cb(void (*func)(unsigned char, unsigned char))
{
    hal.tap_cb = func;
}

void invmpu_register_android_orient_cb(void (*func)(unsigned char))
{
    hal.android_orient_cb = func;
}

void invmpu_register_data_ready_cb(__invmpu_data_ready_cb func)
{
    hal.data_ready_cb = func;
}

/*******************************************************************************/

static void read_from_mpu(void)
{
    unsigned char accel_fsr, new_temp;
    unsigned long timestamp;
    unsigned long sensor_timestamp;
    int sensors;
#ifdef COMPASS_ENABLED
    unsigned char new_compass = 0;
#endif

have_data:
    new_temp = 0;
    sensors = 0;
    get_clock_ms(&timestamp);

   /* Sends a quaternion packet to the PC. Since this is used by the Python
    * test app to visually represent a 3D quaternion, it's sent each time
    * the MPL has new data.
    */
    eMPL_send_quat(hal.quat);

#ifdef COMPASS_ENABLED
    /* We're not using a data ready interrupt for the compass, so we'll
     * make our compass reads timer-based instead.
     */
    if ((timestamp > hal.next_compass_ms) && !hal.lp_accel_mode &&
        hal.new_gyro && (hal.sensors & COMPASS_ON)) {
        hal.next_compass_ms = timestamp + COMPASS_READ_MS;
        new_compass = 1;
    }
#endif
    /* Temperature data doesn't need to be read with every gyro sample.
     * Let's make them timer-based like the compass reads.
     */
    if (timestamp > hal.next_temp_ms) {
        hal.next_temp_ms = timestamp + TEMP_READ_MS;
        new_temp = 1;
    }

    if (hal.motion_int_mode) {
        /* Enable motion interrupt. */
        mpu_lp_motion_interrupt(500, 1, 5);
        /* FIXME: dead lock */
        /* Wait for the MPU interrupt. */
        while (!hal.new_gyro) {}
        /* Restore the previous sensor configuration. */
        mpu_lp_motion_interrupt(0, 0, 0);
        hal.motion_int_mode = 0;
    }

    if (!hal.sensors || !hal.new_gyro) {
        return;
    }

    if (hal.new_gyro && hal.lp_accel_mode) {
        mpu_get_accel_reg(hal.accel_short, &sensor_timestamp);
        hal.new_gyro = 0;
        sensors |= INV_XYZ_ACCEL;
    } else if (hal.new_gyro && hal.dmp_on) {
        unsigned char more = 0;
        /* This function gets new data from the FIFO when the DMP is in
         * use. The FIFO can contain any combination of gyro, accel,
         * quaternion, and gesture data. The sensors parameter tells the
         * caller which data fields were actually populated with new data.
         * For example, if sensors == (INV_XYZ_GYRO | INV_WXYZ_QUAT), then
         * the FIFO isn't being filled with accel data.
         * The driver parses the gesture data to determine if a gesture
         * event has occurred; on an event, the application will be notified
         * via a callback (assuming that a callback function was properly
         * registered). The more parameter is non-zero if there are
         * leftover packets in the FIFO.
         */
        dmp_read_fifo(hal.gyro, hal.accel_short, hal.quat,
                        &sensor_timestamp, (short *)&sensors, &more);
        if (!more)
            hal.new_gyro = 0;
        if (sensors & INV_XYZ_GYRO) {
            if (new_temp) {
                new_temp = 0;
                /* Temperature only used for gyro temp comp. */
                mpu_get_temperature(&hal.temperature, &sensor_timestamp);
            }
        }
    } else if (hal.new_gyro) {
        unsigned char more = 0;
        /* This function gets new data from the FIFO. The FIFO can contain
         * gyro, accel, both, or neither. The sensors parameter tells the
         * caller which data fields were actually populated with new data.
         * For example, if sensors == INV_XYZ_GYRO, then the FIFO isn't
         * being filled with accel data. The more parameter is non-zero if
         * there are leftover packets in the FIFO. The HAL can use this
         * information to increase the frequency at which this function is
         * called.
         */
        hal.new_gyro = 0;
        mpu_read_fifo(hal.gyro, hal.accel_short,
                    &sensor_timestamp, (unsigned char *)&sensors, &more);
        if (more)
            hal.new_gyro = 1;
        if (sensors & INV_XYZ_GYRO) {
            if (new_temp) {
                new_temp = 0;
                /* Temperature only used for gyro temp comp. */
                mpu_get_temperature(&hal.temperature, &sensor_timestamp);
            }
        }
    }
#ifdef COMPASS_ENABLED
    if (new_compass) {
        new_compass = 0;
        /* For any MPU device with an AKM on the auxiliary I2C bus, the raw
         * magnetometer registers are copied to special gyro registers.
         */
        if (!mpu_get_compass_reg(hal.compass_short, &sensor_timestamp)) {
            /* NOTE: If using a third-party compass calibration library,
             * pass in the compass data in uT * 2^16 and set the second
             * parameter to INV_CALIBRATED | acc, where acc is the
             * accuracy from 0 to 3.
             */
            sensors |= INV_XYZ_COMPASS;
        }
    }
#endif

    if (sensors && hal.data_ready_cb) {
        hal.data_ready_cb(sensors, sensor_timestamp, hal.quat,
            hal.accel_short, hal.gyro, hal.compass_short, hal.temperature);
    }

    if (hal.new_gyro)
        goto have_data;
}

static void int_cb(int fd, short what, void *arg)
{
    hal.new_gyro = 1;
    read_from_mpu();
}

int invmpu_init(int pin_int, int sample_rate, char *hostname, long long_port)
{
    int result;
    struct int_param_s int_param;
    int err;

    err = bcm2835_gpio_signal(pin_int, EDGE_both, int_cb, NULL, &hal.ev_int);
    if (err < 0) {
        LOGE("gpio_signal(%d), err = %d\n", pin_int, err);
        return err;
    }

    result = mpu_init(&int_param);
    if (result) {
        LOGE("Could not initialize gyro.\n");
    }
  
    /* If you're not using an MPU9150 AND you're not using DMP features, this
     * function will place all slaves on the primary bus.
     * mpu_set_bypass(1);
     */

    /* Get/set hardware configuration. Start gyro. */
    /* Wake up all sensors. */
#ifdef COMPASS_ENABLED
    err = mpu_set_sensors(INV_XYZ_GYRO | INV_XYZ_ACCEL | INV_XYZ_COMPASS);
#else
    err = mpu_set_sensors(INV_XYZ_GYRO | INV_XYZ_ACCEL);
#endif
    if (err)
        LOGE("mpu_set_sensors() error\n");

    /* Push both gyro and accel data into the FIFO. */
    err = mpu_configure_fifo(INV_XYZ_GYRO | INV_XYZ_ACCEL);
    if (err)
        LOGE("mpu_configure_fifo() error\n");

    err = mpu_set_sample_rate(sample_rate);
    if (err)
        LOGE("mpu_set_sample_rate() error\n");
#ifdef COMPASS_ENABLED
    /* The compass sampling rate can be less than the gyro/accel sampling rate.
     * Use this function for proper power management.
     */
    err = mpu_set_compass_sample_rate(1000 / COMPASS_READ_MS);
    if (err)
        LOGE("mpu_set_compass_sample_rate() error\n");
#endif

    /* Initialize HAL state variables. */
#ifdef COMPASS_ENABLED
    hal.sensors = ACCEL_ON | GYRO_ON | COMPASS_ON;
#else
    hal.sensors = ACCEL_ON | GYRO_ON;
#endif
    hal.dmp_on = 0;
    hal.report = 0;
    hal.next_pedo_ms = 0;
    hal.next_compass_ms = 0;
    hal.next_temp_ms = 0;

    /* To initialize the DMP:
     * 1. Call dmp_load_motion_driver_firmware(). This pushes the DMP image in
     *    inv_mpu_dmp_motion_driver.h into the MPU memory.
     * 2. Push the gyro and accel orientation matrix to the DMP.
     * 3. Register gesture callbacks. Don't worry, these callbacks won't be
     *    executed unless the corresponding feature is enabled.
     * 4. Call dmp_enable_feature(mask) to enable different features.
     * 5. Call dmp_set_fifo_rate(freq) to select a DMP output rate.
     * 6. Call any feature-specific control functions.
     *
     * To enable the DMP, just call mpu_set_dmp_state(1). This function can
     * be called repeatedly to enable and disable the DMP at runtime.
     *
     * The following is a short summary of the features supported in the DMP
     * image provided in inv_mpu_dmp_motion_driver.c:
     * DMP_FEATURE_LP_QUAT: Generate a gyro-only quaternion on the DMP at
     * 200Hz. Integrating the gyro data at higher rates reduces numerical
     * errors (compared to integration on the MCU at a lower sampling rate).
     * DMP_FEATURE_6X_LP_QUAT: Generate a gyro/accel quaternion on the DMP at
     * 200Hz. Cannot be used in combination with DMP_FEATURE_LP_QUAT.
     * DMP_FEATURE_TAP: Detect taps along the X, Y, and Z axes.
     * DMP_FEATURE_ANDROID_ORIENT: Google's screen rotation algorithm. Triggers
     * an event at the four orientations where the screen should rotate.
     * DMP_FEATURE_GYRO_CAL: Calibrates the gyro data after eight seconds of
     * no motion.
     * DMP_FEATURE_SEND_RAW_ACCEL: Add raw accelerometer data to the FIFO.
     * DMP_FEATURE_SEND_RAW_GYRO: Add raw gyro data to the FIFO.
     * DMP_FEATURE_SEND_CAL_GYRO: Add calibrated gyro data to the FIFO. Cannot
     * be used in combination with DMP_FEATURE_SEND_RAW_GYRO.
     */
    err = dmp_load_motion_driver_firmware();
    if (err)
        LOGE("dmp_load_motion_driver_firmware(), err = %d\n", err);

    err = dmp_set_orientation(
        inv_orientation_matrix_to_scalar(gyro_pdata.orientation));
    if (err)
        LOGE("dmp_set_orientation(), err = %d\n", err);

    dmp_register_tap_cb(tap_cb);
    dmp_register_android_orient_cb(android_orient_cb);
    /*
     * Known Bug -
     * DMP when enabled will sample sensor data at 200Hz and output to FIFO at the rate
     * specified in the dmp_set_fifo_rate API. The DMP will then sent an interrupt once
     * a sample has been put into the FIFO. Therefore if the dmp_set_fifo_rate is at 25Hz
     * there will be a 25Hz interrupt from the MPU device.
     *
     * There is a known issue in which if you do not enable DMP_FEATURE_TAP
     * then the interrupts will be at 200Hz even if fifo rate
     * is set at a different rate. To avoid this issue include the DMP_FEATURE_TAP
     *
     * DMP sensor fusion works only with gyro at +-2000dps and accel +-2G
     */
    hal.dmp_features = DMP_FEATURE_6X_LP_QUAT | DMP_FEATURE_TAP |
        DMP_FEATURE_ANDROID_ORIENT | DMP_FEATURE_SEND_RAW_ACCEL | DMP_FEATURE_SEND_CAL_GYRO |
        DMP_FEATURE_GYRO_CAL;
    dmp_enable_feature(hal.dmp_features);

    err = dmp_set_fifo_rate(sample_rate);
    if (err)
        LOGE("dmp_set_fifo_rate(), err = %d\n", err);

    err = mpu_set_dmp_state(1);
    if (err)
        LOGE("mpu_set_dmp_state(), err = %d\n", err);

    hal.dmp_on = 1;

    eMPL_init_connect(hostname, long_port);
}

void invmpu_exit(void)
{
    /* TODO */
    if (hal.ev_int)
        eventfd_del(hal.ev_int);
}
