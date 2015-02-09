#include <stdlib.h>
#include <string.h>

#include "platform.h"


void get_ms(unsigned long *count)
{
    if (count != NULL) {
        *count = clock();
    }
}

int reg_int_cb(struct int_param_s *int_param)
{
    /* FIXME */
    return 0;
}

int set_reg_ptr(unsigned char regptr, size_t size)
{
    if (bcm2835_i2c_write((const char *)&regptr, 1) != 0)
        return -ENOTTY;
    return 0;
}

int i2c_write(unsigned char slave_addr, unsigned char reg_addr,
    unsigned char length, unsigned char const *data)
{
    unsigned char *buf;
    buf = malloc(length + 1);
    if (buf == NULL)
        return -ENOMEM;

    buf[0] = reg_addr;
    memcpy(&buf[1], data, length);

    bcm2835_i2c_setSlaveAddress(slave_addr);
    if (bcm2835_i2c_write((const char *)buf, length + 1) != 0)
        return -ENOTTY;

    free(buf);
    return 0;
}

int i2c_read(unsigned char slave_addr, unsigned char reg_addr,
    unsigned char length, unsigned char *data)
{
    int err;

    err = set_reg_ptr(reg_addr, length);
    if (err < 0)
        return err;
    bcm2835_i2c_setSlaveAddress(slave_addr);
    if (bcm2835_i2c_read((char *)data, length) != 0)
        return -EXDEV;
    return 0;
}
