#ifndef __SOCK_H__
#define __SOCK_H__

#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/un.h>

union sockaddr_u {
    struct sockaddr_storage storage;
    struct sockaddr_un un;
    struct sockaddr_in in;
    struct sockaddr_in6 in6;
    struct sockaddr sockaddr;
};

#endif /* __SOCK_H__ */
