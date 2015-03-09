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
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <time.h>

#include <bcm2835.h>

#ifdef LOG_STD
#define log_i(...)  fprintf(stdout, __VA_ARGS__)
#define log_e(...)  fprintf(stderr, __VA_ARGS__)
#else
#define log_i(...)  do {} while (0)
#define log_e(...)  do {} while (0)
#endif
#define delay_ms    bcm2835_delay
#define min(a, b)   (((a) < (b)) ? (a) : (b))
#define __no_operation()    do {} while (0)

static void get_ms(unsigned long *count)
{
    struct timeval tv;
    if (!count)
        return;
    gettimeofday(&tv, NULL);
    *count = tv.tv_sec * 1000 + tv.tv_usec / 1000;
}

static int reg_int_cb(struct int_param_s *int_param)
{
    /* FIXME */
    return 0;
}

static int set_reg_ptr(unsigned char regptr, size_t size)
{
    if (bcm2835_i2c_write((const char *)&regptr, 1) != 0)
        return -ENOTTY;
    return 0;
}

static int i2c_write(unsigned char slave_addr, unsigned char reg_addr,
    unsigned char length, unsigned char const *data)
{
    unsigned char buffer[64];
    unsigned char *buf;

    int rs;

    if (length <= 63) {
        buf = buffer;
    } else {
        buf = malloc(length + 1);
        if (buf == NULL)
            return -ENOMEM;
    }

    buf[0] = reg_addr;
    memcpy(&buf[1], data, length);

    bcm2835_i2c_setSlaveAddress(slave_addr);

    if ((rs = bcm2835_i2c_write((const char *)buf, length + 1)) != 0) {
        fprintf(stderr, "bcm2835_i2c_write(slave=%x, reg=%x, len=%d), rs = %d\n",
                slave_addr, reg_addr, length, rs);

        /* FIXME */
        /*return -ENOTTY;*/
    }

    if (buf != buffer)
        free(buf);

    return 0;
}

static int i2c_read(unsigned char slave_addr, unsigned char reg_addr,
    unsigned char length, unsigned char *data)
{
    int err;

    bcm2835_i2c_setSlaveAddress(slave_addr);
    err = set_reg_ptr(reg_addr, length);
    if (err < 0)
        return err;
    if (bcm2835_i2c_read((char *)data, length) != 0)
        return -EXDEV;
    return 0;
}
