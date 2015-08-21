#include <stdio.h>
#include <errno.h>
#include <event2/event.h>

#include <inv_mpu.h>
#include <inv_mpu_dmp_motion_driver.h>
#include <ml_math_func.h>
#include <invensense.h>
#include <invensense_adv.h>
#include <eMPL_outputs.h>

#include <bcm2835.h>

#include "event.h"
#include "gpiolib.h"

#include "inv_imu.h"


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
    unsigned char sensors;
    unsigned long next_temp_ms;
    unsigned long next_compass_ms;
    unsigned short dmp_features;

    void (*tap_cb)(unsigned char count, unsigned char direction);
    void (*android_orient_cb)(unsigned char orientation);
    __invmpu_data_ready_cb data_ready_cb;
    struct event *ev_int;    /* event for pin interrupt */
};
static struct hal_s hal = {0};

unsigned char *mpl_key = (unsigned char *)"eMPL 5.1";

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

int _MLPrintLog(int priority, const char *tag, const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    vfprintf(stdout, fmt, ap);
    va_end(ap);
    return 0;
}

/* ---------------------------------------------------------------------------*/
/*
 * The compass rate is never changed.
 */
void invmpu_set_sample_rate(int rate)
{
    dmp_set_fifo_rate(rate);
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

static void read_from_mpl(void)
{
    long quat[4];
    long accel[3];
    long gyro[3];
    long compass[3];
    short sensors = 0;
    int8_t accuracy;
    unsigned long timestamp;

    if (inv_get_sensor_type_quat(quat, &accuracy, (inv_time_t*)&timestamp)) {
        sensors |= INV_WXYZ_QUAT;
    }

    if (inv_get_sensor_type_accel(accel, &accuracy, (inv_time_t*)&timestamp)) {
        sensors |= INV_XYZ_ACCEL;
    }

    if (inv_get_sensor_type_gyro(gyro, &accuracy, (inv_time_t*)&timestamp)) {
        sensors |= INV_XYZ_GYRO;
    }

    if (inv_get_sensor_type_compass(compass, &accuracy, (inv_time_t*)&timestamp)) {
        sensors |= INV_XYZ_COMPASS;
    }

    if (sensors && hal.data_ready_cb) {
        hal.data_ready_cb(sensors, timestamp, quat,
            accel, gyro, compass);
    }
}

static void int_cb(int fd, short what, void *arg)
{
    short gyro[3], accel_short[3], sensors;
    unsigned char more = 0;
    long accel[3], quat[4], temperature;
    unsigned char new_temp = 0;
    unsigned long timestamp;
    unsigned long sensor_timestamp;
    int new_data = 0;
#ifdef COMPASS_ENABLED
    unsigned char new_compass = 0;
#endif

    get_clock_ms(&timestamp);

#ifdef COMPASS_ENABLED
    /* We're not using a data ready interrupt for the compass, so we'll
     * make our compass reads timer-based instead.
     */
    if ((timestamp > hal.next_compass_ms) &&
        (hal.sensors & COMPASS_ON)) {
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

    if (!hal.sensors) {
        return;
    }

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
    dmp_read_fifo(gyro, accel_short, quat,
                    &sensor_timestamp, (short *)&sensors, &more);
    if (sensors & INV_XYZ_GYRO) {
        /* Push the new data to the MPL. */
        inv_build_gyro(gyro, sensor_timestamp);
        new_data = 1;
        if (new_temp) {
            new_temp = 0;
            /* Temperature only used for gyro temp comp. */
            mpu_get_temperature(&temperature, &sensor_timestamp);
            inv_build_temp(temperature, sensor_timestamp);
        }
    }
    if (sensors & INV_XYZ_ACCEL) {
        accel[0] = (long)accel_short[0];
        accel[1] = (long)accel_short[1];
        accel[2] = (long)accel_short[2];
        inv_build_accel(accel, 0, sensor_timestamp);
        new_data = 1;
    }
    if (sensors & INV_WXYZ_QUAT) {
        inv_build_quat(quat, 0, sensor_timestamp);
        new_data = 1;
    }

#ifdef COMPASS_ENABLED
    if (new_compass) {
        short compass_short[3];
        long compass[3];
        new_compass = 0;
        /* For any MPU device with an AKM on the auxiliary I2C bus, the raw
         * magnetometer registers are copied to special gyro registers.
         */
        if (!mpu_get_compass_reg(compass_short, &sensor_timestamp)) {
            compass[0] = (long)compass_short[0];
            compass[1] = (long)compass_short[1];
            compass[2] = (long)compass_short[2];
            /* NOTE: If using a third-party compass calibration library,
             * pass in the compass data in uT * 2^16 and set the second
             * parameter to INV_CALIBRATED | acc, where acc is the
             * accuracy from 0 to 3.
             */
            inv_build_compass(compass, 0, sensor_timestamp);
            new_data = 1;
        }
    }
#endif

    if (new_data) {
        inv_execute_on_data();

        read_from_mpl();
    }
}

int invmpu_init(int pin_int, int sample_rate)
{
    int result;
    struct int_param_s int_param;
    unsigned char accel_fsr;
    unsigned short gyro_rate, gyro_fsr;
    int err;
#ifdef COMPASS_ENABLED
    unsigned short compass_fsr;
#endif

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
    
    result = inv_init_mpl();
    if (result) {
        LOGE("Could not initialize MPL.\n");
    }

    /* Compute 6-axis and 9-axis quaternions. */
    inv_enable_quaternion();
    inv_enable_9x_sensor_fusion();
    /* The MPL expects compass data at a constant rate (matching the rate
     * passed to inv_set_compass_sample_rate). If this is an issue for your
     * application, call this function, and the MPL will depend on the
     * timestamps passed to inv_build_compass instead.
     *
     * inv_9x_fusion_use_timestamps(1);
     */

    /* This function has been deprecated.
     * inv_enable_no_gyro_fusion();
     */

    /* Update gyro biases when not in motion.
     * WARNING: These algorithms are mutually exclusive.
     */
    inv_enable_fast_nomot();
    /* inv_enable_motion_no_motion(); */
    /* inv_set_no_motion_time(1000); */

    /* Update gyro biases when temperature changes. */
    inv_enable_gyro_tc();

    /* This algorithm updates the accel biases when in motion. A more accurate
     * bias measurement can be made when running the self-test (see case 't' in
     * handle_input), but this algorithm can be enabled if the self-test can't
     * be executed in your application.
     *
     * inv_enable_in_use_auto_calibration();
     */
#ifdef COMPASS_ENABLED
    /* Compass calibration algorithms. */
    inv_enable_vector_compass_cal();
    inv_enable_magnetic_disturbance();
#endif
    /* If you need to estimate your heading before the compass is calibrated,
    * enable this algorithm. It becomes useless after a good figure-eight is
    * detected, so we'll just leave it out to save memory.
    * inv_enable_heading_from_gyro();
    */

    /* Allows use of the MPL APIs in read_from_mpl. */
    inv_enable_eMPL_outputs();

    result = inv_start_mpl();
    if (result == INV_ERROR_NOT_AUTHORIZED) {
        while (1) {
            LOGE("Not authorized.\n");
        }
    }
    if (result) {
        LOGE("Could not start the MPL.\n");
    }


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

    /* Read back configuration in case it was set improperly. */
    mpu_get_sample_rate(&gyro_rate);
    mpu_get_gyro_fsr(&gyro_fsr);
    mpu_get_accel_fsr(&accel_fsr);
#ifdef COMPASS_ENABLED
    mpu_get_compass_fsr(&compass_fsr);
#endif
    /* Sync driver configuration with MPL. */
    /* Sample rate expected in microseconds. */
    inv_set_gyro_sample_rate(1000000L / gyro_rate);
    inv_set_accel_sample_rate(1000000L / gyro_rate);
#ifdef COMPASS_ENABLED
    /* The compass rate is independent of the gyro and accel rates. As long as
    * inv_set_compass_sample_rate is called with the correct value, the 9-axis
    * fusion algorithm's compass correction gain will work properly.
    */
    inv_set_compass_sample_rate(COMPASS_READ_MS * 1000L);
#endif
    /* Set chip-to-body orientation matrix.
    * Set hardware units to dps/g's/degrees scaling factor.
    */
    inv_set_gyro_orientation_and_scale(
            inv_orientation_matrix_to_scalar(gyro_pdata.orientation),
            (long)gyro_fsr<<15);
    inv_set_accel_orientation_and_scale(
            inv_orientation_matrix_to_scalar(gyro_pdata.orientation),
            (long)accel_fsr<<15);
#ifdef COMPASS_ENABLED
    inv_set_compass_orientation_and_scale(
            inv_orientation_matrix_to_scalar(compass_pdata.orientation),
            (long)compass_fsr<<15);
#endif
    /* Initialize HAL state variables. */
#ifdef COMPASS_ENABLED
    hal.sensors = ACCEL_ON | GYRO_ON | COMPASS_ON;
#else
    hal.sensors = ACCEL_ON | GYRO_ON;
#endif
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

    return err;
}

void invmpu_exit(void)
{
    /* TODO */
    if (hal.ev_int)
        eventfd_del(hal.ev_int);
}
