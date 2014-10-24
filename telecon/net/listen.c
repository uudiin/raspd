#include <stdio.h>
#include <errno.h>
#include <sys/socket.h>

#include "tree.h"

static fd_set master_readfds;

struct fdinfo {
    int fd;
    union sockaddr_u remoteaddr;
    socklen_t ss_len;
    RB_ENTRY(fdinfo) node;
};

//static RB_HEAD(fdroot, fdinfo) tree_readfds = RB_INITIALIZER(&tree_readfds);
static RB_HEAD(fdroot, fdinfo) tree_clients = RB_INITIALIZER(&tree_readfds);

static int fdinfo_comp(struct fdinfo *f1, struct fdinfo *f2)
{
    return f1->fd - f2->fd;
}

RB_GENERATE_STATIC(fdroot, fdinfo, node, fdinfo_comp);

static void client_add(int fd, union sockaddr_u *sockaddr, socklen_t ss_len)
{
    struct fdinfo *info;
    info = (struct fdinfo *)xmalloc(sizeof(*info));
    memset(info, 0, sizeof(*info));
    info->fd = fd;
    if (sockaddr && ss_len) {
        info->ss_len = ss_len;
        memcpy(&info->remoteaddr, sockaddr, ss_len);
    }
    RB_INSERT(fdroot, &tree_clients, info);
}

static void client_fd_add(int fd)
{
    client_add(fd, NULL, 0);
}


static void handle_connection(int fd_listen)
{
    union sockaddr_u remoteaddr;
    socklen_t ss_len;
    int fd;

    fd = accept(fd_listen, &remoteaddr.sockaddr, &ss_len);
    if (fd < 0)
        return;

    unblock_socket(fd);

    /* TODO  cmdexec */

    FD_SET(STDIN_FILENO, &master_readfds);
    FD_SET(fd, &master_readfds);
    client_fd_add(STDIN_FILENO);
    client_add(fd, &remoteaddr, ss_len);
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
    int fd_listen;
    int err;

    ss_len = sizeof(addr);
    err = resolve("0.0.0.0", portno, &addr.storage, &ss_len, AF_INET, 0);
    if (err < 0)
        return err;

    signal(SIGCHLD, sigchld_handler);
    signal(SIGPIPE, SIG_IGN);

    fd_listen = do_listen(SOCK_STREAM, proto, &addr);
    if (fd_listen == -1)
        return -1;

    unblock_socket(fd_listen);

    FD_ZERO(&master_readfds);
    FD_SET(fd_listen, &master_readfds);

    client_fd_add(fd_listen);

    while (1) {
        struct fdinfo *info;
        fd_set readfds = master_readfds;
        int ready;

        info = RB_MAX(fdroot, &tree_clients);
        ready = select(info->fd + 1, &readfds, NULL, NULL, NULL);
        if (ready == 0)
            continue;

        RB_FOREACH(info, fdroot, &tree_clients) {
            if (!FD_ISSET(info->fd, &readfds))
                continue;

            if (info->fd == fd_listen) {
                handle_connection(fd_listen);
            } else if (info->fd == STDIN_FILENO) {
                /* FIXME */
            }
        }
    }
}
