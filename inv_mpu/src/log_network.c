#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <sys/socket.h>

#include "../../lib/sock.h"
#include "packet.h"
#include "log.h"

static int _use_udp = 0;
static int fd = -1;
static union sockaddr_u addr;

int _MLPrintLog(int priority, const char *tag, const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    vfprintf(stdout, fmt, ap);
    va_end(ap);
    return 0;
}

void eMPL_connect(char *hostname, long long_port, int use_udp)
{
    int err;
    size_t ss_len = sizeof(addr);

    err = resolve(hostname, (unsigned short)long_port,
            &addr.storage, &ss_len, AF_INET, 0);
    if (err < 0) {
        return;
    }

    if (use_udp) {
        fd = socket(addr.storage.ss_family, SOCK_DGRAM, 0);
    } else {
        fd = do_connect(SOCK_STREAM, &addr);
    }

    if (fd >= 0) {
        unblock_socket(fd);
    }

    _use_udp = use_udp;
}

#define BUF_SIZE        (256)
#define PACKET_LENGTH   (23)

#define PACKET_DEBUG    (1)
#define PACKET_QUAT     (2)
#define PACKET_DATA     (3)

void eMPL_send_quat(long *quat)
{
    char out[PACKET_LENGTH];
    ssize_t n;

    if (!quat || fd < 0)
        return;
    memset(out, 0, PACKET_LENGTH);
    out[0] = '$';
    out[1] = PACKET_QUAT;
    out[3] = (char)(quat[0] >> 24);
    out[4] = (char)(quat[0] >> 16);
    out[5] = (char)(quat[0] >> 8);
    out[6] = (char)quat[0];
    out[7] = (char)(quat[1] >> 24);
    out[8] = (char)(quat[1] >> 16);
    out[9] = (char)(quat[1] >> 8);
    out[10] = (char)quat[1];
    out[11] = (char)(quat[2] >> 24);
    out[12] = (char)(quat[2] >> 16);
    out[13] = (char)(quat[2] >> 8);
    out[14] = (char)quat[2];
    out[15] = (char)(quat[3] >> 24);
    out[16] = (char)(quat[3] >> 16);
    out[17] = (char)(quat[3] >> 8);
    out[18] = (char)quat[3];
    out[21] = '\r';
    out[22] = '\n';

    if (_use_udp) {
        size_t sa_len;
#ifdef HAVE_SOCKADDR_SA_LEN
        sa_len = addr.sockaddr.sa_len;
#else
        sa_len = sizeof(addr);
#endif
        n = sendto(fd, out, PACKET_LENGTH, 0, &addr.sockaddr, sa_len);
    } else {
        n = send(fd, out, PACKET_LENGTH, 0);
    }
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
                                  /*case PACKET_DATA_EULER:   PRINT3("EULER: "); break;*/
#undef PRINT3

        case PACKET_DATA_EULER:
                                  fprintf(stdout, "EULER: %.2f %.2f %.2f\n",
                                          data[0] / 65536.f,
                                          data[1] / 65536.f,
                                          data[2] / 65536.f);
                                  break;

        case PACKET_DATA_HEADING:
                                  fprintf(stdout, "HEADING: %ld\n", data[0]);
                                  break;

        default:
                                  return;
    }
}
