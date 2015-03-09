#ifndef __MS5611_H__
#define __MS5611_H__

#include <stdint.h>

struct ms5611_dev {
    struct event *ev;

    uint16_t C1, C2, C3, C4, C5, C6, CRC;

    uint32_t D1, D2;
    int32_t dT, TEMP, P;
    int64_t OFF, SENS;

    double sea_press;
    int step;

    enum last_conv {
        temperature,
        pressure
    } last_conv;

};

struct ms5611_dev *ms5611_new(double sea_press);
void ms5611_del(struct ms5611_dev *dev);

double ms5611_get_pressure(struct ms5611_dev *dev);
double ms5611_get_altitude(struct ms5611_dev *dev);

#endif
