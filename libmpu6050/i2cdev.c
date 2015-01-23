#include "i2cdev.h"

#include <string.h>
#include <bcm2835.h>

int set_reg_ptr(unsigned char regptr, size_t size)
{
    if (bcm2835_i2c_write((const char *)&regptr, 1) != 0)
        return -1;
    return 0;
}

bool i2cdev_writeBytes(uint8_t devAddr, uint8_t regAddr, uint8_t length, uint8_t *data)
{
    uint8_t buf[128];
    if (length > 127)
        return false;
    buf[0] = regAddr;
    memcpy(buf+1,data,length);
    if (bcm2835_i2c_write((const char *)buf, length + 1) != 0)
        return false;
    return true;
}

bool i2cdev_writeByte(uint8_t devAddr, uint8_t regAddr, uint8_t data)
{
    unsigned char buffer[2] = { regAddr, data };
    if (bcm2835_i2c_write((const char *)buffer, 2) != 0)
        return false;
    return true;
}

int8_t i2cdev_readByte(uint8_t devAddr, uint8_t regAddr, uint8_t *data)
{
    int err;

    if ((err = set_reg_ptr(regAddr, 1)) < 0)
        return err;
    if (bcm2835_i2c_read((char *)data, 1) != 0)
        return 0;
    return 1;
}

int8_t i2cdev_readBytes(uint8_t devAddr, uint8_t regAddr, uint8_t length, uint8_t *data)
{
    int err;

    if ((err = set_reg_ptr(regAddr, length)) < 0)
        return err;
    if (bcm2835_i2c_read((char *)data, length) != 0)
        return 0;
    return length;
}

int8_t i2cdev_readBit(uint8_t devAddr, uint8_t regAddr, uint8_t bitNum, uint8_t *data)
{
    uint8_t b;
    uint8_t count = i2cdev_readByte(devAddr, regAddr, &b);
    *data = b & (1 << bitNum);
    return count;
}

bool i2cdev_writeBit(uint8_t devAddr, uint8_t regAddr, uint8_t bitNum, uint8_t data)
{
    uint8_t b;
    i2cdev_readByte(devAddr, regAddr, &b);
    b = (data != 0) ? (b | (1 << bitNum)) : (b & ~(1 << bitNum));
    return i2cdev_writeByte(devAddr, regAddr, b);
}

int8_t i2cdev_readBits(uint8_t devAddr, uint8_t regAddr, uint8_t bitStart, uint8_t length, uint8_t *data)
{
    uint8_t count, b;
    if ((count = i2cdev_readByte(devAddr, regAddr, &b)) != 0) {
        uint8_t mask = ((1 << length) - 1) << (bitStart - length + 1);
        b &= mask;
        b >>= (bitStart - length + 1);
        *data = b;
    }
    return count;
}

bool i2cdev_writeBits(uint8_t devAddr, uint8_t regAddr, uint8_t bitStart, uint8_t length, uint8_t data)
{
    uint8_t b;
    if (i2cdev_readByte(devAddr, regAddr, &b) != 0) {
        uint8_t mask = ((1 << length) - 1) << (bitStart - length + 1);
        data <<= (bitStart - length + 1); // shift data into correct position
        data &= mask; // zero all non-important bits in data
        b &= ~(mask); // zero all important bits in existing byte
        b |= data; // combine data with existing byte
        return i2cdev_writeByte(devAddr, regAddr, b);
    } else {
        return false;
    }
}

bool i2cdev_writeWords(uint8_t devAddr, uint8_t regAddr, uint8_t length, uint16_t* data)
{
    uint8_t buf[128];
    uint8_t i;
    buf[0] = regAddr;
    for (i = 0; i < length; i++) {
        buf[i*2+1] = data[i] >> 8;
        buf[i*2+2] = data[i];
    }
    
    if (bcm2835_i2c_write((const char *)buf, (length * 2) + 1) != 0)
        return false;
    return true;
}

bool i2cdev_writeWord(uint8_t devAddr, uint8_t regAddr, uint16_t data)
{
    return i2cdev_writeWords(devAddr, regAddr, 1, &data);
}
