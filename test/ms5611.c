#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <errno.h>
#include <unistd.h>
#include <math.h>

#include <bcm2835.h>

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

static uint16_t C1, C2, C3, C4, C5, C6, CRC;
static uint32_t D1, D2;

static int32_t dT, TEMP, P;
static int64_t OFF, SENS;

static void reset(void)
{
    uint8_t dummyData = 0;
    i2c_write(MS5611_ADDRESS, resetCommand, 0, &dummyData);
}

static void get_calib(void)
{
    unsigned char buf[2];

    i2c_read(MS5611_ADDRESS, C1_ADDRESS, 2, buf);
    C1 = (buf[0] << 8) | buf[1];
    i2c_read(MS5611_ADDRESS, C2_ADDRESS, 2, buf);
    C2 = (buf[0] << 8) | buf[1];
    i2c_read(MS5611_ADDRESS, C3_ADDRESS, 2, buf);
    C3 = (buf[0] << 8) | buf[1];
    i2c_read(MS5611_ADDRESS, C4_ADDRESS, 2, buf);
    C4 = (buf[0] << 8) | buf[1];
    i2c_read(MS5611_ADDRESS, C5_ADDRESS, 2, buf);
    C5 = (buf[0] << 8) | buf[1];
    i2c_read(MS5611_ADDRESS, C6_ADDRESS, 2, buf);
    C6 = (buf[0] << 8) | buf[1];
    i2c_read(MS5611_ADDRESS, CRC_ADDRESS, 2, buf);
    CRC = (buf[0] << 8) | buf[1];
}

static void calculate_pressure(void)
{
    /* Calculate temperature */
    /* Difference between actual and reference temperature */
    dT = D2 - (C5 << 8);
    TEMP = 2000 + ((dT * C6) >> 23);
    /* Calculate temperature compensated pressure */
    OFF = ((int64_t)C2 << 16) + (((int64_t)C4 * (int64_t)dT) >> 7);
    SENS = (((int64_t)C1) << 15) + (((int64_t)C3 * (int64_t)dT) >> 8);

    /* if temperature lower than 20 Celsius */
    if (TEMP < 2000) {
        int32_t T2    = 0;
        int64_t OFF2  = 0;
        int64_t SENS2 = 0;

        T2    = pow(dT, 2) / (1 << 31);
        OFF2  = 5 * pow((TEMP - 2000), 2) / (1 << 1);
        SENS2 = 5 * pow((TEMP - 2000), 2) / (1 << 2);

        /* if temperature lower than -15 Celsius */ 
        if (TEMP < -1500) {
            OFF2  += 7 * pow((TEMP + 1500), 2); 
            SENS2 += 11 * pow((TEMP + 1500), 2) / (1 << 1);
        }

        TEMP -= T2;
        OFF -= OFF2; 
        SENS -= SENS2;
    }

    P = (((D1 * SENS) >> 21) - OFF) >> 15;
}

void ms5611_init(void)
{
    reset();
    usleep(100000);
    get_calib();
}

void ms5611_getpressure(void)
{
    uint8_t dummyData = 0;
    unsigned char buf[3];

    /* start pressure conversion */
    i2c_write(MS5611_ADDRESS, D1Conv4096, 0, &dummyData);

    usleep(100000);
    /* get raw pressure */
    i2c_read(MS5611_ADDRESS, 0x00, 3, buf);
    D1 = (buf[0] << 16) | (buf[1] << 8) | (buf[2]);

    /* start temp conversion */
    i2c_write(MS5611_ADDRESS, D2Conv4096, 0, &dummyData);

    usleep(100000);
    /* get raw temperature */
    i2c_read(MS5611_ADDRESS, 0x00, 3, buf);
    D2 = (buf[0] << 16) | (buf[1] << 8) | (buf[2]);

    calculate_pressure();
}

static double sea_press = 1025.25;

double ms5611_calcAltitude(int pressure)
{
    double altitude = ((pow((sea_press / ((double)pressure / 100)), 1/5.257) - 1.0)
            * ((TEMP / 100) + 273.15)) / 0.0065;
    return altitude;
}

int main(int argc, char *argv[])
{

    if (!bcm2835_init())
        return 1;
    bcm2835_i2c_begin();
    bcm2835_i2c_setClockDivider(BCM2835_I2C_CLOCK_DIVIDER_626);

    ms5611_init();

    while (1) {

        ms5611_getpressure();

        printf("pressure: %f, temp: %f, altitude: %f\n",
                ((double)P / 100), ((double)TEMP / 100), ms5611_calcAltitude(P));
        usleep(100000);
    }

    bcm2835_i2c_end();
    bcm2835_close();

    return 0;
}
