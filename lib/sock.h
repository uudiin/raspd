#ifndef __SOCK_H__
#define __SOCK_H__

#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/un.h>

#ifndef SUN_LEN
/* Evaluate to actual length of the `sockaddr_un' structure.  */
# define SUN_LEN(ptr) ((size_t) (((struct sockaddr_un *) 0)->sun_path)	      \
		      + strlen ((ptr)->sun_path))
#endif

union sockaddr_u {
    struct sockaddr_storage storage;
    struct sockaddr_un un;
    struct sockaddr_in in;
    struct sockaddr_in6 in6;
    struct sockaddr sockaddr;
};

int resolve(const char *hostname, unsigned short port,
        struct sockaddr_storage *ss, size_t *sslen, int af, int addl_flags);

int unblock_socket(int sd);
int block_socket(int sd);
int do_listen(int type, int proto, const union sockaddr_u *srcaddr);
int do_connect(int type, const union sockaddr_u *dstaddr);
int unixsock_std(const char *unixsock);

#endif /* __SOCK_H__ */
