#ifndef _I2CDEV_H_
#define _I2CDEV_H_

#include <stdbool.h>
#include <stdint.h>

bool i2cdev_writeBytes(uint8_t devAddr, uint8_t regAddr, uint8_t length, uint8_t *data);
bool i2cdev_writeByte(uint8_t devAddr, uint8_t regAddr, uint8_t data);
int8_t i2cdev_readByte(uint8_t devAddr, uint8_t regAddr, uint8_t *data);
int8_t i2cdev_readBytes(uint8_t devAddr, uint8_t regAddr, uint8_t length, uint8_t *data);
int8_t i2cdev_readBit(uint8_t devAddr, uint8_t regAddr, uint8_t bitNum, uint8_t *data);
bool i2cdev_writeBit(uint8_t devAddr, uint8_t regAddr, uint8_t bitNum, uint8_t data);
int8_t i2cdev_readBits(uint8_t devAddr, uint8_t regAddr, uint8_t bitStart, uint8_t length, uint8_t *data);
bool i2cdev_writeBits(uint8_t devAddr, uint8_t regAddr, uint8_t bitStart, uint8_t length, uint8_t data);
bool i2cdev_writeWords(uint8_t devAddr, uint8_t regAddr, uint8_t length, uint16_t* data);
bool i2cdev_writeWord(uint8_t devAddr, uint8_t regAddr, uint16_t data);

#endif /* _I2CDEV_H_ */
