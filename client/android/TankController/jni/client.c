#include <stdio.h>
#include <errno.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <unistd.h>

#include <sock.h>


static int fd = 0;

int send_cmd(char *buf, int bufsize)
{
	return send(fd, buf, bufsize, 0);
}

int recv_response(char *buf, int bufsize)
{
	return recv(fd, buf, bufsize, 0);
}

int connect_stream(const char *hostname, unsigned short portno)
{
    union sockaddr_u addr;
    size_t ss_len;
    fd_set master_readfds;
    int maxfd;
    int err;

    ss_len = sizeof(addr);
    err = resolve(hostname, portno, &addr.storage, &ss_len, AF_INET, 0);
    if (err < 0)
        return err;

    fd = do_connect(SOCK_STREAM, &addr);
    if (fd < 0)
        return -1;

    return err;
}
