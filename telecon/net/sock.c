#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netdb.h>

#include "sock.h"

int resolve(const char *hostname, unsigned short port,
        struct sockaddr_storage *ss, size_t *sslen, int af, int addl_flags)
{
    struct addrinfo hints, *result;
    char portbuf[16];
    int err;

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = af;
    hints.ai_socktype = SOCK_DGRAM;
    hints.ai_flags |= addl_flags;

    err = snprintf(portbuf, sizeof(portbuf), "%hu", port);
    if (err < 0 || (size_t)err >= sizeof(portbuf))
        return EAI_FAIL;

    err = getaddrinfo(hostname, portbuf, &hints, &result);
    if (err)
        return err;
    if (result == NULL)
        return EAI_NONAME;
    if (result->ai_addrlen <= 0
            || result->ai_addrlen > (int)sizeof(struct sockaddr_storage))
        return EAI_SYSTEM;

    *sslen = result->ai_addrlen;
    memcpy(ss, result->ai_addr, *sslen);
    freeaddrinfo(result);

    return 0;
}

int unblock_socket(int sd)
{
    int options;

    options = fcntl(sd, F_GETFL);
    if (options == -1)
        return -1;

    return fcntl(sd, F_SETFL, options | O_NONBLOCK);
}

int block_socket(int sd)
{
    int options;

    options = fcntl(sd, F_SETFL);
    if (options == -1)
        return -1;

    return fcntl(sd, F_SETFL, options & (~O_NONBLOCK));
}

int do_listen(int type, int proto, const union sockaddr_u *srcaddr)
{
    int sock;
    int option = 1;
    size_t sa_len;

    sock = socket(srcaddr->storage.ss_family, type, proto);
    if (sock == -1)
        return -EINVAL;
    setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &option, sizeof(option));

    if (srcaddr->storage.ss_family == AF_UNIX)
        sa_len = SUN_LEN(&srcaddr->un);
    else
#ifdef HAVE_SOCKADDR_SA_LEN
        sa_len = srcaddr->sockaddr.sa_len;
#else
        sa_len = sizeof(*srcaddr);
#endif

    if (bind(sock, &srcaddr->sockaddr, sa_len) < 0)
        return -EFAULT;

    if (type == SOCK_STREAM)
        listen(sock, 10);

    return sock;
}

int do_connect(int type, const union sockaddr_u *dstaddr)
{
    int sock;

    sock = socket(dstaddr->storage.ss_family, type, 0);
    if (sock != -1) {
        size_t sa_len;
        if (dstaddr->storage.ss_family == AF_UNIX)
            sa_len = SUN_LEN(&dstaddr->un);
        else
#ifdef HAVE_SOCKADDR_SA_LEN
            sa_len = dstaddr->sockaddr.sa_len;
#else
            sa_len = sizeof(*dstaddr);
#endif

        if (connect(sock, &dstaddr->sockaddr, sa_len) != -1)
            return sock;
        else if (errno == EINPROGRESS || errno == EAGAIN)
            return sock;
    }

    return -1;
}
