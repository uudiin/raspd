#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <errno.h>
#include <unistd.h>
#include <getopt.h>
#include <sys/socket.h>
#include <arpa/inet.h>

#include <unix.h>
#include <sock.h>
#include <utils.h>

static int listen_mode;
static int daemon;
static int udp;

/*
 * telecon [options] port
 *     --listen      listen mode
 *     --daemon      deamon
 *     --udp         use UDP instead of default TCP
 */

static void usage(FILE *fp)
{
    fprintf(fp,
        "usage:\n"
        "  catnet [options] <host> <port>\n"
        "\n"
        "options:\n"
        "  -l, --listen         run process as a server mode\n"
        "  -e, --exec <file>    specify the real server exec\n"
        "      --daemon         run process as a daemon\n"
        "      --udp            use UDP protocol, default is TCP\n"
        "  -u, --unix <sock>    specify the unix socket file\n"
        "  -h, --help           display this help screen\n"
        );

    _exit(fp != stderr ? EXIT_SUCCESS : EXIT_FAILURE);
}

int main(int argc, char *argv[])
{
    char *hostname = NULL;
    long long_port = 0;
    char *cmdexec = NULL;
    char *unixsock = NULL;
    int opt_index;
    int c;
    static struct option options[] = {
        { "listen",  no_argument,       NULL,    'l' },
        { "exec",    required_argument, NULL,    'e' },
        { "daemon",  no_argument,       &daemon, 1   },
        { "udp",     no_argument,       &udp,    1   },
        { "unix",    required_argument, NULL,    'u' },
        { "help",    no_argument,       NULL,    'h' },
        { 0, 0, 0, 0 }
    };

    while ((c = getopt_long(argc, argv, "le:u:vh", options, &opt_index)) != -1) {
        switch (c) {
        case 'l': listen_mode = 1; break;
        case 'e': cmdexec = optarg; break;
        case 'u': unixsock = optarg; break;
        case 'h':
            usage(stdout);
            break;
        case '?':
            err_exit(1, "Try '--help' for more information.\n");
        default:
            err_exit(2, "Unrecognised option.\n");
        }
    }

    if (daemon) {
        if (daemonize("catnet") < 0)
            err_exit(1, "daemonize() error\n");
    }

    if (unixsock) {
        if (unixsock_std(unixsock) < 0)
            err_exit(1, "unixsock_std() error\n");
    }

    if (optind >= argc)
        err_exit(3, "You must specify a hostname or a port\n");

    while (optind < argc) {
        if (strspn(argv[optind], "0123456789") == strlen(argv[optind])) {
            /* is port number */
            long_port = strtol(argv[optind], NULL, 10);
            if (long_port <= 0 || long_port > 65535)
                err_exit(4, "Invalid port number.\n");
        } else {
            /* TODO use regex to detect URL or IP */
            hostname = argv[optind];
        }
        optind++;
    }

    if (listen_mode)
        return listen_stream((unsigned short)long_port, cmdexec);
    else
        return connect_stream(hostname, (unsigned short)long_port);
}
