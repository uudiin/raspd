#ifndef __SOCK_H__
#define __SOCK_H__

#include <sys/types.h>
#include <sys/socket.h>

union sockaddr_u {
    struct sockaddr_storage storage;
#ifdef HAVE_SYS_UNIX_H
    struct sockaddr_un un;
#endif
    struct sockaddr_in in;
    struct sockaddr_in6 in6;
    struct sockaddr sockaddr;
};

#endif /* __SOCK_H__ */
