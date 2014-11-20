#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <getopt.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <event2/event.h>

#include <bcm2835.h>

#include <unix.h>
#include <sock.h>

#include "module.h"
#include "event.h"

#define BUF_SIZE    1024


static int read_cmdexec(int fd)
{
    char buffer[BUF_SIZE];
    int err;

    err = read(fd, buffer, sizeof(buffer));
    if (err > 0) {
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
    return err;
}

static void cb_read(int fd, short what, void *arg)
{
    struct event *ev = arg;

    if (read_cmdexec(fd) < 0)
        eventfd_del(ev);
}

static void cb_listen(int fd, short what, void *arg)
{
    union sockaddr_u remoteaddr;
    socklen_t ss_len;
    int fd_cli;
    int err;

    ss_len = sizeof(remoteaddr);
    fd_cli = accept(fd, &remoteaddr.sockaddr, &ss_len);
    if (fd_cli < 0) {
        fprintf(stderr, "accept(), errno = %d\n", errno);
        return;
    }

    if (unblock_socket(fd_cli) < 0) {
        close(fd_cli);
        return;
    }
    err = eventfd_add(fd_cli, EV_READ | EV_PERSIST,
                NULL, cb_read, event_self_cbarg(), NULL);
    if (err < 0) {
        fprintf(stderr, "eventfd_add(), err = %d\n", err);
        close(fd_cli);
        return;
    }
}

static void usage(FILE *fp)
{
    fprintf(fp,
        "usage:\n"
        "  raspd [options]\n"
        "\n"
        "options:\n"
        "  -d, --daemon              run process as a daemon\n"
        "  -l, --listen <port>       listen on port use TCP\n"
        "  -u, --unix-listen <sock>  listen on the unix socket file\n"
        "  -e, --logerr <file>       specify the error log file\n"
        "  -h, --help                display this help screen\n"
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
        { "daemon",      no_argument,       NULL, 'd' },
        { "listen",      required_argument, NULL, 'l' },
        { "unix-listen", required_argument, NULL, 'u' },
        { "logerr",      required_argument, NULL, 'e' },
        { "help",        no_argument,       NULL, 'h' },
        { 0, 0, 0, 0 }
    };
    int c;
    int daemon = 0;
    char *unixlisten = NULL;
    char *logerr = NULL;
    long listen_port = 0;
    struct sockaddr_u addr;
    int fd = -1;
    int err;

    while ((c = getopt_long(argc, argv, "dl:u:e:h", options, NULL)) != -1) {
        switch (c) {
        case 'd': daemon = 1; break;
        case 'l': listen_port = strtol(optarg, NULL, 10); break;
        case 'u': unixlisten = optarg; break;
        case 'e': logerr = optarg; break;
        case 'h': usage(stdout); break;
        default:
            usage(stderr);
            break;
        }
    }

    /* if run as daemon ? */
    if (daemon) {
        err = daemonize("raspd");
        if (err < 0) {
            fprintf(stderr, "daemonize(), err = %d\n", err);
            return 1;
        }
    }

    /* initialize event base */
    if (event_init() < 0) {
        fprintf(stderr, "event_init(), err = %d\n", err);
        return 1;
    }

    /* if not daemon, get data from stdin */
    if (!daemon) {
        err = eventfd_add(STDIN_FILENO, EV_READ | EV_PERSIST,
                        NULL, cb_read, event_self_cbarg(), NULL);
        if (err < 0) {
            fprintf(stderr, "eventfd_add(STDIN_FILENO), err = %d\n", err);
            return 1;
        }
    }

    /* redirect stdout to error log file */
    if (logerr) {
        int logfd;
        logfd = open(logerr, O_RDWR | O_CREAT | O_TRUNC, 0660);
        if (logfd) {
            dup2(logfd, STDERR_FILENO);
            close(logfd);
        }
    }

    if (listen_port > 0 && listen_port < 65535) {
        size_t ss_len;

        ss_len = sizeof(addr);
        err = resolve("0.0.0.0", (unsigned short)listen_port,
                            &addr.storage, &ss_len, AF_INET, 0);
        if (err < 0) {
            perror("resolve()\n");
            return 1;
        }

        fd = do_listen(AF_INET, SOCK_STREAM, &addr);
        if (fd < 0) {
            perror("do_listen()\n");
            return 1;
        }
    } else if (unixlisten) {
        /*err = unixsock_listen(unixlisten);*/
        unlink(unixlisten);
        if (strlen(unixlisten) >= sizeof(addr.un.sun_path)) {
            perror("invalid unix socket\n");
            return 1;
        }

        memset(&addr, 0, sizeof(addr));
        addr.un.sun_family = AF_UNIX;
        strncpy(addr.un.sun_path, unixlisten, sizeof(addr.un.sun_path));

        fd = do_listen(AF_UNIX, SOCK_STREAM, &addr);
        if (fd < 0) {
            perror("do_listen()\n");
            return 1;
        }
    }

    if (fd >= 0) {
        unblock_socket(fd);
        err = eventfd_add(fd, EV_READ | EV_PERSIST,
                    NULL, cb_listen, event_self_cbarg(), NULL);
        if (err < 0) {
            perror("eventfd_add()\n");
            return 1;
        }
    }

    if (!bcm2835_init()) {
        fprintf(stderr, "bcm2835_init() error\n");
        return 1;
    }

    /* main loop */
    do_event_loop();

    bcm2835_close();
    event_exit();

    return 0;
}
