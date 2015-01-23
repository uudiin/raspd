// I2Cdev library collection - MPU6050 I2C device class, 6-axis MotionApps 2.0 implementation
// Based on InvenSense MPU-6050 register map document rev. 2.0, 5/19/2011 (RM-MPU-6000A-00)
// 6/18/2012 by Jeff Rowberg <jeff@rowberg.net>
// Updates should (hopefully) always be available at https://github.com/jrowberg/i2cdevlib
//
// Changelog:
//     ... - ongoing debug release

/* ============================================
I2Cdev device library code is placed under the MIT license
Copyright (c) 2012 Jeff Rowberg

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.
===============================================
*/

#ifndef _MPU6050_6AXIS_MOTIONAPPS20_H_
#define _MPU6050_6AXIS_MOTIONAPPS20_H_

#include <stdint.h>
#include <stdbool.h>
#include "helper_3dmath.h"

/* Source is from the InvenSense MotionApps v2 demo code. Original source is
 * unavailable, unless you happen to be amazing as decompiling binary by
 * hand (in which case, please contact me, and I'm totally serious).
 *
 * Also, I'd like to offer many, many thanks to Noah Zerkin for all of the
 * DMP reverse-engineering he did to help make this bit of wizardry
 * possible.
 */

/* ================================================================================================ *
 | Default MotionApps v2.0 42-byte FIFO packet structure:                                           |
 |                                                                                                  |
 | [QUAT W][      ][QUAT X][      ][QUAT Y][      ][QUAT Z][      ][GYRO X][      ][GYRO Y][      ] |
 |   0   1   2   3   4   5   6   7   8   9  10  11  12  13  14  15  16  17  18  19  20  21  22  23  |
 |                                                                                                  |
 | [GYRO Z][      ][ACC X ][      ][ACC Y ][      ][ACC Z ][      ][      ]                         |
 |  24  25  26  27  28  29  30  31  32  33  34  35  36  37  38  39  40  41                          |
 * ================================================================================================ */

#define prog_uchar uint8_t
#define PROGMEM

uint8_t mpu6050_dmpInitialize();
bool mpu6050_dmpPacketAvailable();

uint8_t mpu6050_dmpSetFIFORate(uint8_t fifoRate);
uint8_t mpu6050_dmpGetFIFORate();
uint8_t mpu6050_dmpGetSampleStepSizeMS();
uint8_t mpu6050_dmpGetSampleFrequency();
int32_t mpu6050_dmpDecodeTemperature(int8_t tempReg);

// Register callbacks after a packet of FIFO data is processed
//uint8_t dmpRegisterFIFORateProcess(inv_obj_func func, int16_t priority);
//uint8_t dmpUnregisterFIFORateProcess(inv_obj_func func);
uint8_t mpu6050_dmpRunFIFORateProcesses();

// Setup FIFO for various output
uint8_t mpu6050_dmpSendQuaternion(uint_fast16_t accuracy);
uint8_t mpu6050_dmpSendGyro(uint_fast16_t elements, uint_fast16_t accuracy);
uint8_t mpu6050_dmpSendAccel(uint_fast16_t elements, uint_fast16_t accuracy);
uint8_t mpu6050_dmpSendLinearAccel(uint_fast16_t elements, uint_fast16_t accuracy);
uint8_t mpu6050_dmpSendLinearAccelInWorld(uint_fast16_t elements, uint_fast16_t accuracy);
uint8_t mpu6050_dmpSendControlData(uint_fast16_t elements, uint_fast16_t accuracy);
uint8_t mpu6050_dmpSendSensorData(uint_fast16_t elements, uint_fast16_t accuracy);
uint8_t mpu6050_dmpSendExternalSensorData(uint_fast16_t elements, uint_fast16_t accuracy);
uint8_t mpu6050_dmpSendGravity(uint_fast16_t elements, uint_fast16_t accuracy);
uint8_t mpu6050_dmpSendPacketNumber(uint_fast16_t accuracy);
uint8_t mpu6050_dmpSendQuantizedAccel(uint_fast16_t elements, uint_fast16_t accuracy);
uint8_t mpu6050_dmpSendEIS(uint_fast16_t elements, uint_fast16_t accuracy);

// Get Fixed Point data from FIFO
uint8_t mpu6050_dmpGetAccel32(int32_t *data, const uint8_t* packet);
uint8_t mpu6050_dmpGetAccel16(int16_t *data, const uint8_t* packet);
uint8_t mpu6050_dmpGetAccel(struct VectorInt16 *v, const uint8_t* packet);
uint8_t mpu6050_dmpGetQuaternion32(int32_t *data, const uint8_t* packet);
uint8_t mpu6050_dmpGetQuaternion16(int16_t *data, const uint8_t* packet);
uint8_t mpu6050_dmpGetQuaternion(struct Quaternion *q, const uint8_t* packet);
uint8_t mpu6050_dmpGetGyro32(int32_t *data, const uint8_t* packet);
uint8_t mpu6050_dmpGetGyro16(int16_t *data, const uint8_t* packet);
uint8_t mpu6050_dmpSetLinearAccelFilterCoefficient(float coef);
uint8_t mpu6050_dmpGetLinearAccel(struct VectorInt16 *v, struct VectorInt16 *vRaw, struct VectorFloat *gravity);
uint8_t mpu6050_dmpGetLinearAccelInWorld(struct VectorInt16 *v, struct VectorInt16 *vReal, struct Quaternion *q);
uint8_t mpu6050_dmpGetGravity(struct VectorFloat *v, struct Quaternion *q);

uint8_t mpu6050_dmpGetEuler(float *data, struct Quaternion *q);
uint8_t mpu6050_dmpGetYawPitchRoll(float *data, struct Quaternion *q, struct VectorFloat *gravity);

uint8_t mpu6050_dmpProcessFIFOPacket(const unsigned char *dmpData);
uint8_t mpu6050_dmpReadAndProcessFIFOPacket(uint8_t numPackets, uint8_t *processed);

uint8_t mpu6050_dmpSetFIFOProcessedCallback(void (*func) (void));

uint8_t mpu6050_dmpInitFIFOParam();
uint8_t mpu6050_dmpCloseFIFO();
uint8_t mpu6050_dmpSetGyroDataSource(uint8_t source);
uint8_t mpu6050_dmpDecodeQuantizedAccel();
uint32_t mpu6050_dmpGetGyroSumOfSquare();
uint32_t mpu6050_dmpGetAccelSumOfSquare();
void mpu6050_dmpOverrideQuaternion(long *q);
uint16_t mpu6050_dmpGetFIFOPacketSize();

#endif /* _MPU6050_6AXIS_MOTIONAPPS20_H_ */
