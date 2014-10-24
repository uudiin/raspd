#include <stdio.h>
#include <errno.h>
#include <sys/socket.h>

static void handle_connection(int socket_accept)
{
    union sockaddr_u remoteaddr;
    socklen_t ss_len
    int fd;

    fd = accept(socket_accept, &remoteaddr.sockaddr, &ss_len);
    if (fd < 0)
}

int do_listen(int type, int proto, const union sockaddr_u *srcaddr)
{
    int sock;
    int option = 1;
    size_t sa_len;

    sock = socket(srcaddr->storage.ss_family, type, proto);
    if (sock == -1)
        return sock;
    setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &option, sizeof(option));

    if (srcaddr->storage.ss_family == AF_UNIX)
        sa_len = SUN_LEN(&srcaddr->un);
    else
        sa_len = srcaddr->sockaddr.sa_len;

    if (bind(sock, &srcaddr->sockaddr, sa_len) < 0)
        return -EFAULT;

    if (type == SOCK_STREAM)
        listen(sock, 10);

    return sock;
}

static void sigchld_handler(int signum)
{
    while (waitpid(-1, NULL, WNOHANG) > 0)
        xxx;
}

int listen_stream(int portno)
{
    union sockaddr_u addr;
    size_t ss_len;
    int sock;
    fd_set master_readfds, master_writefds;
    int err;

    ss_len = sizeof(addr);
    err = resolve("0.0.0.0", portno, &addr.storage, &ss_len, AF_INET, 0);
    if (err < 0)
        return err;

    signal(SIGCHLD, sigchld_handler);
    signal(SIGPIPE, SIG_IGN);

    sock = do_listen(SOCK_STREAM, proto, &addr);
    if (sock == -1)
        return -1;

    unblock_socket(sock);

    FD_ZERO(&master_readfds);
    FD_ZERO(&master_writefds);
    FD_SET(sock, &master_readfds);
    while (1) {
        fd_set readfds = master_readfds;
        fd_set writefds = master_writefds;
        int ready;

        ready = select(sock + 1, &readfds, &writefds, NULL, NULL);
        if (ready == 0)
            continue;

        if (FD_ISSET(sock, &readfds)) {
            handle_connection(sock);
        }
    }
}
