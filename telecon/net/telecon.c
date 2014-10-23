#include <stdio.h>
#include <stdarg.h>
#include <errno.h>

static int listen_mode;
static int deamon;
static int udp;

static union sockaddr_u srcaddr;
static size_t srcaddrlen;
static union sockaddr_u targetss;
static size_t targetsslen;

void err_exit(int retval, const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
    exit(retval);
}

/*
 * telecon [options] port
 *     --listen      listen mode
 *     --daemon      deamon
 *     --udp         use UDP instead of default TCP
 */

int main(int argc, char *argv[])
{
    static struct option options[] = {
        { "listen",  required_argument, NULL,    'l' },
        { "daemon",  no_argument,       &deamon, 1   },
        { "udp",     no_argument,       &udp,    1   },
        { "version", no_argument,       NULL,    'v' },
        { "help",    no_argument,       NULL,    'h' },
        { 0, 0, 0, 0 }
    };
    long long_port;
    int opt_index;
    int c;

    while ((c = getopt_long(argc, argv, "l:", options, &opt_index)) != -1) {
        switch (c) {
        case 'l':
            listen_mode = 1;
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

    if (optind == argc)
        err_exit(3, "You must specify a port to listen\n");

    if (strspn(argv[optind], "0123456789") != strlen(argv[optind]))
        err_exit(3, "Illegal port number.\n");

    long_port = strtol(argv[optind], NULL, 10);
    if (long_port <= 0 || long_port > 65535)
        err_exit(4, "Invalid port number.\n");

    return 0;
}
