#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <math.h>

#include <bcm2835.h>

#include "event.h"
#include "ms5611.h"

static int set_reg_ptr(unsigned char regptr, size_t size)
{
    if (bcm2835_i2c_write((const char *)&regptr, 1) != 0)
        return -ENOTTY;
    return 0;
}

static int i2c_write(unsigned char slave_addr, unsigned char reg_addr,
        unsigned char length, unsigned char const *data)
{
    unsigned char *buf;
    buf = malloc(length + 1);
    if (buf == NULL) {
        return -ENOMEM;
    }

    buf[0] = reg_addr;
    memcpy(&buf[1], data, length);

    bcm2835_i2c_setSlaveAddress(slave_addr);
    if (bcm2835_i2c_write((const char *)buf, length + 1) != 0) {
        free(buf);
        return -ENOTTY;
    }

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

const uint8_t MS5611_ADDRESS = 0x77;
const uint8_t C0_ADDRESS = 0xA0;
const uint8_t C1_ADDRESS = 0xA2;
const uint8_t C2_ADDRESS = 0xA4;
const uint8_t C3_ADDRESS = 0xA6;
const uint8_t C4_ADDRESS = 0xA8;
const uint8_t C5_ADDRESS = 0xAA;
const uint8_t C6_ADDRESS = 0xAC;
const uint8_t CRC_ADDRESS = 0xAE;
const uint8_t D1Conv4096 = 0x48;
const uint8_t D2Conv4096 = 0x58;
const uint8_t resetCommand = 0x1E;

static void reset(void)
{
    uint8_t dummyData = 0;
    i2c_write(MS5611_ADDRESS, resetCommand, 0, &dummyData);
}

static void get_calib(struct ms5611_dev *dev)
{
    unsigned char buf[2];

    i2c_read(MS5611_ADDRESS, C1_ADDRESS, 2, buf);
    dev->C1 = (buf[0] << 8) | buf[1];
    i2c_read(MS5611_ADDRESS, C2_ADDRESS, 2, buf);
    dev->C2 = (buf[0] << 8) | buf[1];
    i2c_read(MS5611_ADDRESS, C3_ADDRESS, 2, buf);
    dev->C3 = (buf[0] << 8) | buf[1];
    i2c_read(MS5611_ADDRESS, C4_ADDRESS, 2, buf);
    dev->C4 = (buf[0] << 8) | buf[1];
    i2c_read(MS5611_ADDRESS, C5_ADDRESS, 2, buf);
    dev->C5 = (buf[0] << 8) | buf[1];
    i2c_read(MS5611_ADDRESS, C6_ADDRESS, 2, buf);
    dev->C6 = (buf[0] << 8) | buf[1];
    i2c_read(MS5611_ADDRESS, CRC_ADDRESS, 2, buf);
    dev->CRC = (buf[0] << 8) | buf[1];
}

static void calculate_pressure(struct ms5611_dev *dev)
{
    /* Calculate temperature */
    /* Difference between actual and reference temperature */
    dev->dT = dev->D2 - (dev->C5 << 8);
    dev->TEMP = 2000 + ((dev->dT * dev->C6) >> 23);
    /* Calculate temperature compensated pressure */
    dev->OFF = ((int64_t)dev->C2 << 16) + (((int64_t)dev->C4 * (int64_t)dev->dT) >> 7);
    dev->SENS = (((int64_t)dev->C1) << 15) + (((int64_t)dev->C3 * (int64_t)dev->dT) >> 8);

    /* if temperature lower than 20 Celsius */
    if (dev->TEMP < 2000) {
        int32_t T2    = 0;
        int64_t OFF2  = 0;
        int64_t SENS2 = 0;

        T2    = pow(dev->dT, 2) / (1 << 31);
        OFF2  = 5 * pow((dev->TEMP - 2000), 2) / (1 << 1);
        SENS2 = 5 * pow((dev->TEMP - 2000), 2) / (1 << 2);

        /* if temperature lower than -15 Celsius */ 
        if (dev->TEMP < -1500) {
            OFF2  += 7 * pow((dev->TEMP + 1500), 2); 
            SENS2 += 11 * pow((dev->TEMP + 1500), 2) / (1 << 1);
        }

        dev->TEMP -= T2;
        dev->OFF -= OFF2; 
        dev->SENS -= SENS2;
    }

    dev->P = (((dev->D1 * dev->SENS) >> 21) - dev->OFF) >> 15;
}

double calc_altitude(struct ms5611_dev *dev)
{
    double altitude = ((pow((dev->sea_press / ((double)dev->P / 100)), 1/5.257) - 1.0)
            * ((dev->TEMP / 100) + 273.15)) / 0.0065;
    return altitude;
}

static void cb_get_pressure(int fd, short what, void *arg)
{
	struct timeval tv = {0, 250};
	struct ms5611_dev *dev = arg;
    uint8_t dummyData = 0;
    unsigned char buf[3];

    switch (dev->step) {

    case 0:
        /* start temp conversion */
        i2c_write(MS5611_ADDRESS, D2Conv4096, 0, &dummyData);
        dev->step++;
        break;
    case 1:
        /* get raw temperature */
        i2c_read(MS5611_ADDRESS, 0x00, 3, buf);
        dev->D2 = (buf[0] << 16) | (buf[1] << 8) | (buf[2]);
        /* start pressure conversion */
        i2c_write(MS5611_ADDRESS, D1Conv4096, 0, &dummyData);
        dev->step++;
        break;
    case 2:
        /* get raw pressure */
        i2c_read(MS5611_ADDRESS, 0x00, 3, buf);
        dev->D1 = (buf[0] << 16) | (buf[1] << 8) | (buf[2]);
        calculate_pressure(dev);
        dev->step = 0;
        break;
    default:
        break;
    }

    evtimer_add(dev->ev, &tv);
}

struct ms5611_dev *ms5611_new(double sea_press)
{
    struct ms5611_dev *dev;

    dev = malloc(sizeof(*dev));
    if (dev) {
        uint8_t dummyData = 0;

        memset(dev, 0, sizeof(*dev));
        dev->sea_press = sea_press;
        reset();
        usleep(100000);
        get_calib(dev);

        dev->ev = evtimer_new(evbase, cb_get_pressure, dev);
        if (dev->ev == NULL) {
            free(dev);
            return NULL;
        }
    }

    return dev;
}

void ms5611_del(struct ms5611_dev *dev)
{
    if (dev) {
        if (dev->ev)
            eventfd_del(dev->ev);
        free(dev);
    }
}

double ms5611_get_pressure(struct ms5611_dev *dev)
{
    return (double)dev->P / 100;
}

double ms5611_get_altitude(struct ms5611_dev *dev)
{
    return calc_altitude(dev);
}
