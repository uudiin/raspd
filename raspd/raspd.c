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
#include "event.h"

#define BUF_SIZE    1024

struct raspd_struct {
    int listen_fd;
    int unix_listen_fd;
    struct event *listen_ev;
    struct event *unix_listen_ev;
};

static struct raspd_struct raspd;

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

static void cb_read_stdin(int fd, short what, void *arg)
{
    /* FIXME */
    read(STDIN_FILENO
}

static void cb_listen(int fd, short what, void *arg)
{
    /* FIXME */
    accept
}

static void cb_conn_read(struct bufferevent *bev, void *arg)
{ }
static void cb_conn_write(struct bufferevent *bev, void *arg)
{ }
static void cb_conn_event(struct bufferevent *bev, short events, void *arg)
{ }
static void cb_listener(struct evconnlistener *listener, int fd,
                        struct sockaddr *sa, int socklen, void *arg)
{ }

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
    char buffer[BUF_SIZE];
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
                        NULL, cb_read_stdin, event_self_cbarg(), NULL);
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

    err = 0;
    if (listen_port > 0 && listen_port < 65535) {
        /*err = stream_listen((unsigned short)listen_port);*/
        struct sockaddr_u addr;
        size_t ss_len;

        ss_len = sizeof(addr);
        err = resolve("0.0.0.0", (unsigned short)listen_port,
                            &addr.storage, &ss_len, AF_INET, 0);
        if (err < 0) {
            /*XXX*/
            return 1;
        }

        fd = do_listen(AF_INET, SOCK_STREAM, &addr);
        if (fd < 0) {
            /*XXX*/
            return 1;
        }
        unblock_socket(fd);
        err = eventfd_add(fd, EV_READ | EV_PERSIST,
                    NULL, cb_listen, event_self_cbarg(), NULL);
        /* FIXME */
    } else if (unixlisten) {
        err = unixsock_listen(unixlisten);
    }

    if (err < 0) {
        fprintf(stderr, "listen error, unixlisten = %s, port = %d, errno = %d\n",
                    unixlisten ? unixlisten : "NULL", (int)listen_port, errno);
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
