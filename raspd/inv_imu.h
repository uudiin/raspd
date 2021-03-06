#ifndef __INV_IMU_H__
#define __INV_IMU_H__

typedef void (*__invmpu_data_ready_cb)(short sensors, unsigned long timestamp,
                    long quat[], long accel[], long gyro[],
                    long compass[]);

void invmpu_self_test(void);
int invmpu_get_calibrate_data(long gyro[], long accel[]);
void invmpu_set_calibrate_data(long gyro[], long accel[]);
void invmpu_set_dmp_state(int dmp_on);
void invmpu_set_sample_rate(int rate);
void invmpu_register_tap_cb(void (*func)(unsigned char, unsigned char));
void invmpu_register_android_orient_cb(void (*func)(unsigned char));
void invmpu_register_data_ready_cb(__invmpu_data_ready_cb func);
int invmpu_init(int pin_int, int sample_rate);
void invmpu_exit(void);

#endif /* __INV_IMU_H__ */
