#include <stdio.h>
#include <errno.h>

static int resolve(const char *hostname, unsigned short port,
                        struct sockaddr_storage *ss, size_t *sslen,
                        int af, int addl_flags)
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
    if (result->ai_addrlen <= 0 || result->ai_addrlen > (int)sizeof(struct sockaddr_storage))
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
