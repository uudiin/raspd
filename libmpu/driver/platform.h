/* The following functions must be defined for this platform:
 * i2c_write(unsigned char slave_addr, unsigned char reg_addr,
 *      unsigned char length, unsigned char const *data)
 * i2c_read(unsigned char slave_addr, unsigned char reg_addr,
 *      unsigned char length, unsigned char *data)
 * delay_ms(unsigned long num_ms)
 * get_ms(unsigned long *count)
 * reg_int_cb(void (*cb)(void), unsigned char port, unsigned char pin)
 * labs(long x)
 * fabsf(float x)
 * min(int a, int b)
 */
#ifndef __PLATFORM_H__
#define __PLATFORM_H__

#include <time.h>
#include <errno.h>

#include <bcm2835.h>

#include "inv_mpu.h"

#define MPU6050

#define log_i(...)  do {} while(0);
#define log_e(...)  do {} while(0);

#define delay_ms    bcm2835_delay

#define min(a,b) ((a<b)?a:b)

#define __no_operation() do {} while(0);

void get_ms(unsigned long *count);
int reg_int_cb(struct int_param_s *int_param);
int set_reg_ptr(unsigned char regptr, size_t size);
int i2c_write(unsigned char slave_addr, unsigned char reg_addr,
    unsigned char length, unsigned char const *data);
int i2c_read(unsigned char slave_addr, unsigned char reg_addr,
    unsigned char length, unsigned char *data);

#endif /* __PLATFORM_H__ */
