/**
 *   @defgroup  eMPL
 *   @brief     Embedded Motion Processing Library
 *
 *   @{
 *       @file      mllite_test.c
 *       @brief     Test app for eMPL using the Motion Driver DMP image.
 */
 
/* Includes ------------------------------------------------------------------*/
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <time.h>
#include <math.h>
#include <assert.h>
#include <getopt.h>
#include <termios.h>
#include <unistd.h>
#include <sys/socket.h>
#include <event2/event.h>

#include "mltypes.h"
#include "inv_mpu.h"
#include "inv_mpu_dmp_motion_driver.h"

#include <bcm2835.h>
#include "../../lib/sock.h"
#include "../../raspd/event.h"
#include "../../raspd/gpiolib.h"

static int fd = -1;

/* Private typedef -----------------------------------------------------------*/
#undef LOGE
#undef LOGI
#define LOGE(...)   fprintf(stderr, __VA_ARGS__)
#define LOGI(...)   fprintf(stdout, __VA_ARGS__)
/* Data read from MPL. */
#define PRINT_ACCEL     (0x01)
#define PRINT_GYRO      (0x02)
#define PRINT_QUAT      (0x04)
#define PRINT_COMPASS   (0x08)
#define PRINT_EULER     (0x10)
#define PRINT_ROT_MAT   (0x20)
#define PRINT_HEADING   (0x40)
#define PRINT_PEDO      (0x80)
#define PRINT_LINEAR_ACCEL (0x100)

#define ACCEL_ON        (0x01)
#define GYRO_ON         (0x02)
#define COMPASS_ON      (0x04)

#define MOTION          (0)
#define NO_MOTION       (1)

/* Starting sampling rate. */
#define DEFAULT_MPU_HZ  (20)

#define FLASH_SIZE      (512)
#define FLASH_MEM_START ((void*)0x1800)

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
#ifdef COMPASS_ENABLED
    short compass_short[3];
#endif
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
int get_clock_ms(unsigned long *count)
{
    struct timeval tv;
    if (!count)
        return -1;
    gettimeofday(&tv, NULL);
    *count = tv.tv_sec * 1000 + tv.tv_usec / 1000;
    return 0;
}

#define BUF_SIZE        (256)
#define PACKET_LENGTH   (23)

#define PACKET_DEBUG    (1)
#define PACKET_QUAT     (2)
#define PACKET_DATA     (3)


void eMPL_send_quat(long *quat)
{
    char out[PACKET_LENGTH];
    int i;
    if (!quat)
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

    if (fd != -1) {
        if (send(fd, out, PACKET_LENGTH, 0) == -1)
            LOGE("send error, errno = %d\n", errno);
    }
}

typedef enum {
    PACKET_DATA_ACCEL = 0,
    PACKET_DATA_GYRO,
    PACKET_DATA_COMPASS,
    PACKET_DATA_QUAT,
    PACKET_DATA_EULER,
    PACKET_DATA_ROT,
    PACKET_DATA_HEADING,
    PACKET_DATA_LINEAR_ACCEL,
    NUM_DATA_PACKETS
} eMPL_packet_e;

void eMPL_send_data(unsigned char type, long *data)
{
    int i;
    switch (type) {
    case PACKET_DATA_ROT:
        fprintf(stdout, "ROT:");
        for (i = 0; i < 8; i++)
            fprintf(stdout, " %ld", data[i] >> 16);
        fprintf(stdout, "\n");
        break;

    case PACKET_DATA_QUAT:
        fprintf(stdout, "QUAT: %ld %ld %ld %ld\n",
                data[0], data[1], data[2], data[3]);
        break;

#define PRINT3(x) fprintf(stdout, x "%ld %ld %ld\n", data[0], data[1], data[2])
    case PACKET_DATA_ACCEL:   PRINT3("ACCEL: "); break;
    case PACKET_DATA_GYRO:    PRINT3("GYRO:  "); break;
    case PACKET_DATA_COMPASS: PRINT3("COMPASS:"); break;
    case PACKET_DATA_EULER:   PRINT3("EULER: "); break;
#undef PRINT3

    case PACKET_DATA_HEADING:
        fprintf(stdout, "HEADING: %ld\n", data[0]);
        break;

    default:
        return;
    }
}
/* ---------------------------------------------------------------------------*/
#ifndef M_PI
#define M_PI 3.14159265358979
#endif

long q29_mult(long a, long b)
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
/* Get data.
 * TODO: Add return values to the inv_get_sensor_type_xxx APIs to differentiate
 * between new and stale data.
 */
static void read_from_mpl(void)
{
    long msg, data[9];
    int8_t accuracy;
    unsigned long timestamp;
    float float_data[3] = {0};

   /* Sends a quaternion packet to the PC. Since this is used by the Python
    * test app to visually represent a 3D quaternion, it's sent each time
    * the MPL has new data.
    */
    eMPL_send_quat(hal.quat);

    /* Specific data packets can be sent or suppressed using USB commands. */
    if (hal.report & PRINT_QUAT)
        eMPL_send_data(PACKET_DATA_QUAT, hal.quat);

    if (hal.report & PRINT_ACCEL) {
        long accel[3];
        accel[0] = (long)hal.accel_short[0];
        accel[1] = (long)hal.accel_short[1];
        accel[2] = (long)hal.accel_short[2];
        eMPL_send_data(PACKET_DATA_ACCEL, accel);
    }
    if (hal.report & PRINT_GYRO) {
        long gyro[3];
        gyro[0] = (long)hal.gyro[0];
        gyro[1] = (long)hal.gyro[1];
        gyro[2] = (long)hal.gyro[2];
        eMPL_send_data(PACKET_DATA_GYRO, gyro);
    }
#ifdef COMPASS_ENABLED
    if (hal.report & PRINT_COMPASS) {
        long compass[3];
        compass[0] = (long)hal.compass_short[0];
        compass[1] = (long)hal.compass_short[1];
        compass[2] = (long)hal.compass_short[2];
        eMPL_send_data(PACKET_DATA_COMPASS, compass);
    }
#endif
    if (hal.report & PRINT_EULER) {
        long euler[3];
        quat_to_euler(hal.quat, euler);
        eMPL_send_data(PACKET_DATA_EULER, euler);
    }
    if (hal.report & PRINT_PEDO) {
        unsigned long timestamp;
        get_clock_ms(&timestamp);
        if (timestamp > hal.next_pedo_ms) {
            hal.next_pedo_ms = timestamp + PEDO_READ_MS;
            unsigned long step_count, walk_time;
            dmp_get_pedometer_step_count(&step_count);
            dmp_get_pedometer_walk_time(&walk_time);
            LOGI("Walked %ld steps over %ld milliseconds..\n", step_count, walk_time);
        }
    }
}

#ifdef COMPASS_ENABLED
void send_status_compass() {
	long data[3] = { 0 };
    data[0] = (long)hal.compass_short[0];
    data[1] = (long)hal.compass_short[1];
    data[2] = (long)hal.compass_short[2];
	LOGI("Compass: %7.4f %7.4f %7.4f ",
			data[0]/65536.f, data[1]/65536.f, data[2]/65536.f);
}
#endif

/* Handle sensor on/off combinations. */
static void setup_gyro(void)
{
    unsigned char mask = 0, lp_accel_was_on = 0;
    if (hal.sensors & ACCEL_ON)
        mask |= INV_XYZ_ACCEL;
    if (hal.sensors & GYRO_ON) {
        mask |= INV_XYZ_GYRO;
        lp_accel_was_on |= hal.lp_accel_mode;
    }
#ifdef COMPASS_ENABLED
    if (hal.sensors & COMPASS_ON) {
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
    if (lp_accel_was_on) {
        hal.lp_accel_mode = 0;
    }
}

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


static inline void run_self_test(void)
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
        	gyro[i] = (long)(gyro[i] * 32.8f);  /* convert to +-1000dps */
        	accel[i] *= 4096.f;                 /* convert to +-8G */
        	accel[i] = accel[i] >> 16;
        	gyro[i] = (long)(gyro[i] >> 16);
        }

        LOGI("cal_gyro  = { %ld, %ld, %ld }\n"
             "cal_accel = { %ld, %ld, %ld }\n",
             gyro[0], gyro[1], gyro[2],
             accel[0], accel[1], accel[2]);

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

static void handle_input(void)
{
    char buffer[32];
    size_t size;
    char c;

    size = read(STDIN_FILENO, buffer, sizeof(buffer));
    if (size < 1)
        return;
    c = buffer[0];

    switch (c) {
    /* These commands turn off individual sensors. */
    case '8':
        hal.sensors ^= ACCEL_ON;
        setup_gyro();
        break;
    case '9':
        hal.sensors ^= GYRO_ON;
        setup_gyro();
        break;
#ifdef COMPASS_ENABLED
    case '0':
        hal.sensors ^= COMPASS_ON;
        setup_gyro();
        break;
#endif
    /* The commands send individual sensor data or fused data to the PC. */
    case 'a':
        hal.report ^= PRINT_ACCEL;
        break;
    case 'g':
        hal.report ^= PRINT_GYRO;
        break;
#ifdef COMPASS_ENABLED
    case 'c':
        hal.report ^= PRINT_COMPASS;
        break;
#endif
    case 'e':
        hal.report ^= PRINT_EULER;
        break;
    case 'r':
        hal.report ^= PRINT_ROT_MAT;
        break;
    case 'q':
        hal.report ^= PRINT_QUAT;
        break;
    case 'h':
        hal.report ^= PRINT_HEADING;
        break;
    case 'i':
        hal.report ^= PRINT_LINEAR_ACCEL;
        break;
#ifdef COMPASS_ENABLED
	case 'w':
		send_status_compass();
		break;
#endif
    /* This command prints out the value of each gyro register for debugging.
     * If logging is disabled, this function has no effect.
     */
    case 'd':
        mpu_reg_dump();
        break;
    /* Test out low-power accel mode. */
    case 'p':
        if (hal.dmp_on)
            /* LP accel is not compatible with the DMP. */
            break;
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
        break;
    /* This snippet of code shows how to load and store calibration data from
     * the MPL. The original code is intended for a MSP430, the flash segment 
     * must be unlocked before reading/writing and locked when no longer in use. 
     * When porting to a different microcontroller, flash memory might be 
     * accessible at anytime, or may not be available at all.
     */
    /*case 'l':
        inv_get_mpl_state_size(&store_size);
        if (store_size > FLASH_SIZE) {
            LOGE("Calibration data exceeds available memory.\n");
            break;
        }
        FCTL3 = FWKEY;
        inv_load_mpl_states(FLASH_MEM_START, store_size);
        FCTL3 = FWKEY + LOCK;
        inv_accel_was_turned_off();
        inv_gyro_was_turned_off();
        inv_compass_was_turned_off();
        break;
    case 's':
        inv_get_mpl_state_size(&store_size);
        if (store_size > FLASH_SIZE) {
            LOGE("Calibration data exceeds available memory.\n");
            return;
        } else {
            unsigned char mpl_states[100], tries = 5, erase_result;
            inv_save_mpl_states(mpl_states, store_size);
            while (tries--) {
                // Multiple attempts to erase current data. 
                Flash_SegmentErase((uint16_t*)FLASH_MEM_START);
                erase_result = Flash_EraseCheck((uint16_t*)FLASH_MEM_START,
                    store_size>>1);
                if (erase_result == FLASH_STATUS_OK)
                    break;
            }
            if (erase_result == FLASH_STATUS_ERROR) {
                LOGE("Could not erase user page for calibration "
                    "storage.\n");
                break;
            }
            FlashWrite_8(mpl_states, FLASH_MEM_START, store_size);
        }
        inv_accel_was_turned_off();
        inv_gyro_was_turned_off();
        inv_compass_was_turned_off();
        break;*/
    /* The hardware self test can be run without any interaction with the
     * MPL since it's completely localized in the gyro driver. Logging is
     * assumed to be enabled; otherwise, a couple LEDs could probably be used
     * here to display the test results.
     */
    case 't':
        run_self_test();
        break;
    /* Depending on your application, sensor data may be needed at a faster or
     * slower rate. These commands can speed up or slow down the rate at which
     * the sensor data is pushed to the MPL.
     *
     * In this example, the compass rate is never changed.
     */
    case '1':
        if (hal.dmp_on) {
            dmp_set_fifo_rate(5);
        } else
            mpu_set_sample_rate(5);
        break;
    case '2':
        if (hal.dmp_on) {
            dmp_set_fifo_rate(20);
        } else
            mpu_set_sample_rate(20);
        break;
    case '3':
        if (hal.dmp_on) {
            dmp_set_fifo_rate(50);
        } else
            mpu_set_sample_rate(50);
        break;
    case '4':
        if (hal.dmp_on) {
            dmp_set_fifo_rate(100);
        } else
            mpu_set_sample_rate(100);
        break;
    case '5':
        if (hal.dmp_on) {
            dmp_set_fifo_rate(200);
        } else
            mpu_set_sample_rate(200);
        break;
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
    case 'f':
        if (hal.lp_accel_mode)
            /* LP accel is not compatible with the DMP. */
            return;
        /* Toggle DMP. */
        if (hal.dmp_on) {
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
        break;
    case 'm':
        /* Test the motion interrupt hardware feature. */
#ifndef MPU6050 /* not enabled for 6050 product */
		hal.motion_int_mode = 1;
#endif 
        break;

    case 'v':
        /* Toggle LP quaternion.
         * The DMP features can be enabled/disabled at runtime. Use this same
         * approach for other features.
         */
        hal.dmp_features ^= DMP_FEATURE_6X_LP_QUAT;
        dmp_enable_feature(hal.dmp_features);
        if (!(hal.dmp_features & DMP_FEATURE_6X_LP_QUAT)) {
            LOGI("LP quaternion disabled.\n");
        } else
            LOGI("LP quaternion enabled.\n");
        break;
    default:
        break;
    }
}

/* Every time new gyro data is available, this function is called in an
 * ISR context. In this example, it sets a flag protecting the FIFO read
 * function.
 */
void gyro_data_ready_cb(void)
{
    hal.new_gyro = 1;
}
/*******************************************************************************/

static struct termios old, new;

static void init_termios(int echo)
{
	tcgetattr(0, &old); /* grab old terminal i/o settings */
	new = old; /* make new settings same as old settings */
	new.c_lflag &= ~ICANON; /* disable buffered i/o */
	new.c_lflag &= echo ? ECHO : ~ECHO; /* set echo mode */
	tcsetattr(0, TCSANOW, &new); /* use these new terminal i/o settings now */
}

/* Restore old terminal i/o settings */
static void reset_termios(void)
{
	tcsetattr(0, TCSANOW, &old);
}


static void once_loop(void)
{
    unsigned char accel_fsr, new_temp = 0;
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
        /* Wait for the MPU interrupt. */
        while (!hal.new_gyro) {}
        /* Restore the previous sensor configuration. */
        mpu_lp_motion_interrupt(0, 0, 0);
        hal.motion_int_mode = 0;
    }

    if (!hal.sensors || !hal.new_gyro) {
        /*continue;*/
        return;
    }

    if (hal.new_gyro && hal.lp_accel_mode) {
        mpu_get_accel_reg(hal.accel_short, &sensor_timestamp);
        new_data = 1;
        hal.new_gyro = 0;
    } else if (hal.new_gyro && hal.dmp_on) {
        short sensors;
        unsigned char more;
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
                        &sensor_timestamp, &sensors, &more);
        if (!more)
            hal.new_gyro = 0;
        if (sensors & INV_XYZ_GYRO) {
            new_data = 1;
            if (new_temp) {
                new_temp = 0;
                /* Temperature only used for gyro temp comp. */
                mpu_get_temperature(&hal.temperature, &sensor_timestamp);
            }
        }
        if (sensors & INV_XYZ_ACCEL) {
            new_data = 1;
        }
        if (sensors & INV_WXYZ_QUAT) {
            new_data = 1;
        }
    } else if (hal.new_gyro) {
        unsigned char sensors, more;
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
                    &sensor_timestamp, &sensors, &more);
        if (more)
            hal.new_gyro = 1;
        if (sensors & INV_XYZ_GYRO) {
            new_data = 1;
            if (new_temp) {
                new_temp = 0;
                /* Temperature only used for gyro temp comp. */
                mpu_get_temperature(&hal.temperature, &sensor_timestamp);
            }
        }
        if (sensors & INV_XYZ_ACCEL) {
            new_data = 1;
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
        }
        new_data = 1;
    }
#endif
    if (new_data) {
        /* This function reads bias-compensated sensor data and sensor
         * fusion outputs from the MPL. The outputs are formatted as seen
         * in eMPL_outputs.c. This function only needs to be called at the
         * rate requested by the host.
         */
        read_from_mpl();
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

static void stdin_ready(int fd, short what, void *arg)
{
    handle_input();
    once_loop();
}

static void int_ready(int fd, short what, void *arg)
{
    gyro_data_ready_cb();
    once_loop();
}

/**
  * @brief main entry point.
  * @par Parameters None
  * @retval void None
  * @par Required preconditions: None
  */
int main(int argc, char *argv[])
{
    inv_error_t result;
    unsigned char accel_fsr,  new_temp = 0;
    unsigned short gyro_rate, gyro_fsr;
    unsigned long timestamp;
    struct int_param_s int_param;
    int err;
#ifdef COMPASS_ENABLED
    unsigned short compass_fsr;
#endif
    char *hostname = NULL;
    long long_port = 8899;
    int pin_int = 17;
    int frequency = DEFAULT_MPU_HZ;
    int self_test = 0;
    static struct option options[] = {
        { "pin-int", required_argument, NULL, 'p' },
        { "hz",      required_argument, NULL, 'f' },
        { "self-test",     no_argument, NULL, 's' },
        { 0, 0, 0, 0 }
    };
    int c;

    while ((c = getopt_long(argc, argv, "p:f:", options, NULL)) != -1) {
        switch (c) {
        case 'p': pin_int = atoi(optarg); break;
        case 'f': frequency = atoi(optarg); break;
        case 's': self_test = 1; break;
        default:
            LOGE("usage: %s --pin-int <n> [hostname] [port]\n", argv[0]);
            exit(1);
        }
    }

    /* send quat data by socket */
    while (optind < argc) {
        if (strspn(argv[optind], "0123456789") == strlen(argv[optind])) {
            /* is port number */
            long_port = strtol(argv[optind], NULL, 10);
            if (long_port < 0 || long_port > 65535) {
                LOGE("Invalid port number.");
                exit(1);
            }
        } else {
            hostname = argv[optind];
        }
        optind++;
    }

    if (hostname && long_port) {
        union sockaddr_u addr;
        size_t ss_len = sizeof(addr);

        err = resolve(hostname, (unsigned short)long_port,
                        &addr.storage, &ss_len, AF_INET, 0);
        if (err < 0) {
            LOGE("Error hostname\n");
            exit(1);
        }

        if ((fd = do_connect(SOCK_STREAM, &addr)) < 0) {
            LOGE("Error connect\n");
        } else {
            unblock_socket(fd);
            LOGI("connected  %s:%ld\n", hostname, long_port);
        }
    }

    init_termios(0);
    if (!bcm2835_init())
        return 1;
    gpiolib_init();
    rasp_event_init();
    bcm2835_i2c_begin();
    bcm2835_i2c_setClockDivider(64);

    err = eventfd_add(STDIN_FILENO, EV_READ | EV_PERSIST,
                    NULL, stdin_ready, NULL, NULL);
    assert(err >= 0);
    /* FIXME:  both */
    err = bcm2835_gpio_signal(pin_int, EDGE_both, int_ready, NULL, NULL);
    assert(err >= 0);
    /* TODO: needed ?
    if (fd != -1) {
        err = eventfd_add(fd, EV_READ | EV_PERSIST,
                        NULL, cli_ready, NULL, NULL);
        assert(err >= 0);
    }
    */
 
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
    mpu_set_sensors(INV_XYZ_GYRO | INV_XYZ_ACCEL | INV_XYZ_COMPASS);
#else
    mpu_set_sensors(INV_XYZ_GYRO | INV_XYZ_ACCEL);
#endif
    /* Push both gyro and accel data into the FIFO. */
    mpu_configure_fifo(INV_XYZ_GYRO | INV_XYZ_ACCEL);
    mpu_set_sample_rate(frequency);
#ifdef COMPASS_ENABLED
    /* The compass sampling rate can be less than the gyro/accel sampling rate.
     * Use this function for proper power management.
     */
    mpu_set_compass_sample_rate(1000 / COMPASS_READ_MS);
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

    /* Compass reads are handled by scheduler. */
    get_clock_ms(&timestamp);

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
    dmp_load_motion_driver_firmware();
    dmp_set_orientation(
        inv_orientation_matrix_to_scalar(gyro_pdata.orientation));
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
    dmp_set_fifo_rate(frequency);
    mpu_set_dmp_state(1);
    hal.dmp_on = 1;

    /* self test, get calibrated data */
    if (self_test) {
        int result;
        long gyro[3], accel[3];

        result = invmpu_get_calibrate_data(gyro, accel);
        if (result == 0) {
            LOGE("Passed!\n");
            LOGE("accel: %7.4f %7.4f %7.4f\n",
                        accel[0]/65536.f,
                        accel[1]/65536.f,
                        accel[2]/65536.f);
            LOGE("gyro: %7.4f %7.4f %7.4f\n",
                        gyro[0]/65536.f,
                        gyro[1]/65536.f,
                        gyro[2]/65536.f);

            fprintf(stdout,
                "-- automatically generated, do not edit\n"
                "\n"
                "cal_gyro  = { %ld, %ld, %ld }\n"
                "cal_accel = { %ld, %ld, %ld }\n",
                gyro[0], gyro[1], gyro[2],
                accel[0], accel[1], accel[2]);

            goto exit;
        } else {
            if (!(result & 0x1))
                LOGE("Gyro failed.\n");
            if (!(result & 0x2))
                LOGE("Accel failed.\n");
            if (!(result & 0x4))
                LOGE("Compass failed.\n");

            goto exit;
        }
    }

    /* main loop */
    rasp_event_loop();

exit:
    bcm2835_i2c_end();
    rasp_event_exit();
    gpiolib_exit();
    bcm2835_close();
    reset_termios();

    return 0;
}
