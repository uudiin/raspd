#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

#include "packet.h"
#include "log.h"

int _MLPrintLog(int priority, const char *tag, const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    vfprintf(stdout, fmt, ap);
    va_end(ap);
    return 0;
}

void eMPL_send_quat(long *quat)
{
    /* fprintf(stdout, "quat: %ld %ld %ld %ld\n",
            quat[0], quat[1], quat[2], quat[3]);
    */
}

void eMPL_send_data(unsigned char type, long *data)
{
    int i;
    switch (type) {
    case PACKET_DATA_ROT:
        fprintf(stdout, "ROT:");
        for (i = 0; i < 8; i++)
            fprintf(stdout, " %ld", data[i] >> 16);
        fprintf(stdout, "\n");
        break;

    case PACKET_DATA_QUAT:
        fprintf(stdout, "QUAT: %ld %ld %ld %ld\n",
                data[0], data[1], data[2], data[3]);
        break;

#define PRINT3(x) fprintf(stdout, x "%ld %ld %ld\n", data[0], data[1], data[2])
    case PACKET_DATA_ACCEL:   PRINT3("ACCEL: "); break;
    case PACKET_DATA_GYRO:    PRINT3("GYRO:  "); break;
    case PACKET_DATA_COMPASS: PRINT3("COMPASS:"); break;
    case PACKET_DATA_EULER:   PRINT3("EULER: "); break;
#undef PRINT3

    case PACKET_DATA_HEADING:
        fprintf(stdout, "HEADING: %ld\n", data[0]);
        break;

    default:
        return;
    }
}
