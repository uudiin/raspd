#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <getopt.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/socket.h>

#include <bcm2835.h>

#include <unix.h>
#include <sock.h>

#include "module.h"

#define BUF_SIZE    1024

static int listen_fork_loop(union sockaddr_u *addr)
{
    union sockaddr_u remoteaddr;
    socklen_t len;
    int fd, clifd;
    int err;

    fd = do_listen(SOCK_STREAM, IPPROTO_TCP/* XXX 0 */, addr);
    if (fd < 0)
        return fd;

    while (1) {
        fd_set readfds;
        int ready;
        pid_t pid;

        FD_ZERO(&readfds);
        FD_SET(fd, &readfds);

        ready = select(fd + 1, &readfds, NULL, NULL, NULL);
        if (ready == 0)
            continue;
        if (!FD_ISSET(fd, &readfds))
            continue;

        len = sizeof(remoteaddr);
        clifd = accept(fd, &remoteaddr.sockaddr, &len);
        if (clifd < 0) {
            fprintf(stderr, "accept() error, errno = %d\n", errno);
            continue;
        }

        if ((pid = fork()) < 0) {
            fprintf(stderr, "fork() error, errno= %d\n", errno);
            close(clifd);
            continue;
        } else if (pid > 0) {
            /* parent */
            /* FIXME  need? */
            /*close(clifd);*/
            continue;
        }

        /* child */
        break;
    }

    /* FIXME close ? */
    /*close(fd);*/

    /* overwrite stdin & stdout */
    dup2(clifd, STDIN_FILENO);
    dup2(clifd, STDOUT_FILENO);
    close(clifd);

    /* no buffer */
    setvbuf(stdin, NULL, _IONBF, 0);
    setvbuf(stdout, NULL, _IONBF, 0);

    return 0;
}

static int stream_listen(unsigned short portno)
{
    union sockaddr_u addr;
    size_t ss_len;
    int err;

    ss_len = sizeof(addr);
    err = resolve("0.0.0.0", portno, &addr.storage, &ss_len, AF_INET, 0);
    if (err < 0)
        return err;

    return listen_fork_loop(&addr);
}

static int unixsock_listen(const char *unixsock)
{
    union sockaddr_u addr;
    socklen_t len;
    int err;

    unlink(unixsock);

    if (strlen(unixsock) >= sizeof(addr.un.sun_path))
        return -EINVAL;

    memset(&addr, 0, sizeof(addr));
    addr.un.sun_family = AF_UNIX;
    strncpy(addr.un.sun_path, unixsock, sizeof(addr.un.sun_path));

    return listen_fork_loop(&addr);
}

static void usage(FILE *fp)
{
    fprintf(fp,
        "usage:\n"
        "  raspd [options]\n"
        "\n"
        "options:\n"
        "  -d, --daemon         run process as a daemon\n"
        "  -s, --listen <port>  listen on port use TCP\n"
        "  -u, --unix <sock>    specify the unix socket file\n"
        "  -l, --logerr <file>  specify the error log file\n"
        "  -h, --help           display this help screen\n"
        );

    _exit(fp != stderr ? EXIT_SUCCESS : EXIT_FAILURE);
}

/*
 * protocol
 *
 *      name xxx yyy zzz; name2 xxxfff
 *      name yyy=xxx fff
 *      cmd1; cmd2; cmd3; ...
 */

int main(int argc, char *argv[])
{
    static struct option options[] = {
        { "daemon",  no_argument,       NULL, 'd' },
        { "listen",  required_argument, NULL, 's' },
        { "unix",    required_argument, NULL, 'u' },
        { "logerr",  required_argument, NULL, 'l' },
        { "help",    no_argument,       NULL, 'h' },
        { 0, 0, 0, 0 }
    };
    int c;
    int daemon = 0;
    char *unixsock = NULL;
    char *logerr = NULL;
    long listen_port = 0;
    char buffer[BUF_SIZE];
    int err;

    while ((c = getopt_long(argc, argv, "ds:u:l:h", options, NULL)) != -1) {
        switch (c) {
        case 'd': daemon = 1; break;
        case 's': listen_port = strtol(optarg, NULL, 10); break;
        case 'u': unixsock = optarg; break;
        case 'l': logerr = optarg; break;
        case 'h': usage(stdout); break;
        default:
            usage(stderr);
            break;
        }
    }

    if (daemon) {
        err = daemonize("raspd");
        if (err < 0) {
            fprintf(stderr, "daemonize(), err = %d\n", err);
            return 1;
        }
    }

    if (logerr) {
        int logfd;
        logfd = open(logerr, O_RDWR | O_CREAT | O_TRUNC, 0660);
        if (logfd) {
            dup2(logfd, STDERR_FILENO);
            close(logfd);
        }
    }

    err = 0;
    if (listen_port > 0 && listen_port < 65535)
        err = stream_listen((unsigned short)listen_port);
    else if (unixsock)
        err = unixsock_listen(unixsock);

    if (err < 0) {
        fprintf(stderr, "listen error, unixsock = %s, port = %d, errno = %d\n",
                    unixsock ? unixsock : "NULL", (int)listen_port, errno);
        return 1;
    }

    if (!bcm2835_init()) {
        fprintf(stderr, "bcm2835_init() error\n");
        return 1;
    }

    /* main loop */
    while (fgets(buffer, sizeof(buffer), stdin) != NULL) {
        char *str, *cmdexec, *saveptr;

        for (str = buffer; cmdexec = strtok_r(str, ";", &saveptr); str = NULL) {

            err = module_cmdexec(cmdexec);
            /* must reply */
            if (err == 0)
                fprintf(stdout, "OK\n");
            else
                fprintf(stdout, "ERR %d\n", err);
        }
    }

    bcm2835_close();

    return 0;
}
