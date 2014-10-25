#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <errno.h>
#include <unistd.h>
#include <getopt.h>
#include <sys/socket.h>
#include <arpa/inet.h>

#include "utils.h"

static int listen_mode;
static int deamon;
static int udp;

/*
 * telecon [options] port
 *     --listen      listen mode
 *     --daemon      deamon
 *     --udp         use UDP instead of default TCP
 */

int main(int argc, char *argv[])
{
    char *hostname = NULL;
    long long_port = 0;
    char *cmdexec = 0;
    int opt_index;
    int c;
    static struct option options[] = {
        { "listen",  no_argument,       NULL,    'l' },
        { "exec",    required_argument, NULL,    'e' },
        { "daemon",  no_argument,       &deamon, 1   },
        { "udp",     no_argument,       &udp,    1   },
        { "version", no_argument,       NULL,    'v' },
        { "help",    no_argument,       NULL,    'h' },
        { 0, 0, 0, 0 }
    };

    while ((c = getopt_long(argc, argv, "le:vh", options, &opt_index)) != -1) {
        switch (c) {
        case 'l':
            listen_mode = 1;
            break;
        case 'e':
            cmdexec = optarg;
            break;
        case 'v':
            break;
        case 'h':
            break;
        case '?':
            err_exit(1, "Try '--help' for more information.\n");
        default:
            err_exit(2, "Unrecognised option.\n");
        }
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
            /* use regex to detect URL or IP */
            hostname = argv[optind];
        }
        optind++;
    }

    if (listen_mode)
        listen_stream((unsigned short)long_port, cmdexec);
    else
        connect_stream(hostname, (unsigned short)long_port);

    return 0;
}
