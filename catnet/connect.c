#include <stdio.h>
#include <errno.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <unistd.h>

#include <sock.h>

#include "catnet.h"


int connect_stream(const char *hostname, unsigned short portno)
{
    union sockaddr_u addr;
    size_t ss_len;
    int fd;
    fd_set master_readfds;
    int maxfd;
    int err;

    ss_len = sizeof(addr);
    err = resolve(hostname, portno, &addr.storage, &ss_len, AF_INET, 0);
    if (err < 0)
        return err;

    /*do {
        char buf[128];
        fprintf(stdout, "host is %s\n",
                inet_ntop(AF_INET, &addr.in.sin_addr, buf, sizeof(buf)));
    } while (0); */

    fd = do_connect(SOCK_STREAM, &addr);
    if (fd < 0)
        return -1;

    unblock_socket(fd);

    FD_ZERO(&master_readfds);
    FD_SET(fd, &master_readfds);
    FD_SET(STDIN_FILENO, &master_readfds);
    maxfd = STDIN_FILENO;
    if (maxfd < fd)
        maxfd = fd;

    while (1) {
        fd_set readfds;
        char buffer[DEFAULT_TCP_BUFLEN];
        int nbytes, n;
        int ready;

        readfds = master_readfds;
        ready = select(maxfd + 1, &readfds, NULL, NULL, NULL);
        if (ready == 0)
            continue;

        if (FD_ISSET(STDIN_FILENO, &readfds)) {
            nbytes = read(STDIN_FILENO, buffer, sizeof(buffer));
            if (nbytes <= 0) {
                FD_CLR(STDIN_FILENO, &readfds);
                err = -1;
                break;
            }

            block_socket(fd);
            n = send(fd, buffer, nbytes, 0);
            unblock_socket(fd);
        } else if (FD_ISSET(fd, &readfds)) {
            n = recv(fd, buffer, sizeof(buffer), 0);
            if (n <= 0) {
                close(fd);
                FD_CLR(fd, &readfds);
                break;
            }
            write(STDOUT_FILENO, buffer, n);
        }
    }

    return err;
}
