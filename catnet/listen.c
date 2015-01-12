#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <unistd.h>
#include <signal.h>

#include <tree.h>
#include <sock.h>
#include <unix.h>
#include <xmalloc.h>

#include "catnet.h"

static fd_set master_readfds;

struct fdinfo {
    int fd;
    union sockaddr_u remoteaddr;
    socklen_t ss_len;
    RB_ENTRY(fdinfo) node;
};

static RB_HEAD(fdroot, fdinfo) tree_clients = RB_INITIALIZER(&tree_readfds);

static int fdinfo_comp(struct fdinfo *f1, struct fdinfo *f2)
{
    return f1->fd - f2->fd;
}

RB_GENERATE_STATIC(fdroot, fdinfo, node, fdinfo_comp);

static struct fdinfo *client_fd_find(int fd)
{
    struct fdinfo temp, *ret;
    temp.fd = fd;
    ret = RB_FIND(fdroot, &tree_clients, &temp);
    return ret;
}

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

static int clients_broadcast(const char *buffer, size_t size)
{
    struct fdinfo *info;
    int retval;
    int err = 0;

    RB_FOREACH(info, fdroot, &tree_clients) {
        if (info->ss_len == 0)
            continue;

        /* blocking send */
        block_socket(info->fd);
        retval = send(info->fd, buffer, size, 0);
        unblock_socket(info->fd);
        if (retval < 0)
            err = -1;
    }

    return err;
}

static void clients_shutdown(int how)
{
    struct fdinfo *info;
    RB_FOREACH(info, fdroot, &tree_clients) {
        if (info->ss_len == 0)
            continue;
        shutdown(info->fd, how);
    }
}


static void handle_connection(int fd_listen, const char *cmdexec)
{
    union sockaddr_u remoteaddr;
    socklen_t ss_len;
    int fd;

    ss_len = sizeof(remoteaddr);
    fd = accept(fd_listen, &remoteaddr.sockaddr, &ss_len);
    if (fd < 0) {
        fprintf(stderr, "accept() error, err = %d\n", errno);
        return;
    }

    unblock_socket(fd);

    /* TODO  cmdexec */
    if (cmdexec) {
        netrun(fd, cmdexec);
    } else {
        FD_SET(STDIN_FILENO, &master_readfds);
        FD_SET(fd, &master_readfds);
        client_fd_add(STDIN_FILENO);
        client_add(fd, &remoteaddr, ss_len);
    }
}

static int read_stdin(void)
{
    char buffer[DEFAULT_TCP_BUFLEN];
    int nbytes;

    nbytes = read(STDIN_FILENO, buffer, sizeof(buffer));
    if (nbytes <= 0) {
        FD_CLR(STDIN_FILENO, &master_readfds);
        return nbytes;
    }

    clients_broadcast(buffer, nbytes);

    return nbytes;
}

static int read_socket(int fd_recv)
{
    struct fdinfo *info;
    char buffer[DEFAULT_TCP_BUFLEN];
    int n = 0;

    info = client_fd_find(fd_recv);
    if (info == NULL)
        return -1;

    n = recv(fd_recv, buffer, sizeof(buffer), 0);
    if (n <= 0) {
        close(fd_recv);
        FD_CLR(fd_recv, &master_readfds);
        RB_REMOVE(fdroot, &tree_clients, info);

        return n;
    }

    write(STDOUT_FILENO, buffer, n);

    return n;
}

static void sigchld_handler(int signum)
{
    while (waitpid(-1, NULL, WNOHANG) > 0);
}

int listen_stream(unsigned short portno, const char *cmdexec)
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

    fd_listen = do_listen(SOCK_STREAM, IPPROTO_TCP, &addr);
    if (fd_listen < 0)
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
                handle_connection(fd_listen, cmdexec);
            } else if (info->fd == STDIN_FILENO) {
                err = read_stdin();
                if (err == 0)
                    clients_shutdown(SHUT_WR);
                else if (err < 0)
                    return err;
            } else {
                err = read_socket(info->fd);
                if (err < 0)
                    return err;
            }

            ready--;
        }
    }

    return 0;
}
