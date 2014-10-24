#include <stdio.h>
#include <errno.h>
#include <sys/socket.h>

int do_connect(int type, const union sockaddr_u *dstaddr)
{
    int sock;

    sock = socket(dstaddr->storage.ss_family, type, 0);
    if (sock != -1) {
        size_t sa_len;
        if (dstaddr->storage.ss_family == AF_UNIX)
            sa_len = SUN_LEN(&dstaddr->un);
        else
            sa_len = dstaddr->sockaddr.sa_len;

        if (connect(sock, &dstaddr.sockaddr, sa_len) != -1)
            return sock;
        else if (socket_errno() == EINPROGRESS || socket_errno() == EAGAIN)
            return sock;
    }

    return -1;
}
