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

int unix_listen(const char *unixsock)
{
    union sockaddr_u addr, remoteaddr;
    socklen_t len = sizeof(remoteaddr);
    int fd, clifd;
    int err;

    unlink(unixsock);

    if (strlen(unixsock) >= sizeof(addr.un.sun_path))
        return -EINVAL;

    memset(&addr, 0, sizeof(addr));
    addr.un.sun_family = AF_UNIX;
    strncpy(addr.un.sun_path, unixsock, sizeof(addr.un.sun_path));

    fd = do_listen(SOCK_STREAM, 0, &addr);
    if (fd < 0)
        return fd;

    while (1) {
        fd_set readfds;
        int ready;

        FD_ZERO(&readfds);
        FD_SET(fd, &readfds);

        ready = select(fd + 1, &readfds, NULL, NULL, NULL);
        if (ready == 0)
            continue;
        if (!FD_ISSET(fd, &readfds))
            continue;

        /* XXX only one client */
        break;
    }

    clifd = accept(fd, &remoteaddr.sockaddr, &len);
    if (clifd < 0)
        return -EFAULT;

    return clifd;
}

static void usage(FILE *fp)
{
    fprintf(fp,
        "usage:\n"
        "  raspd [options]\n"
        "\n"
        "options:\n"
        "  -d, --daemon         run process as a daemon\n"
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
 */

int main(int argc, char *argv[])
{
    static struct option options[] = {
        { "daemon",  no_argument,       NULL, 'd' },
        { "unix",    required_argument, NULL, 'u' },
        { "logerr",  required_argument, NULL, 'l' },
        { "help",    no_argument,       NULL, 'h' },
        { 0, 0, 0, 0 }
    };
    int c;
    int daemon = 0;
    char *unixsock = NULL;
    char *logerr = NULL;
    char buffer[BUF_SIZE];
    int err;

    while ((c = getopt_long(argc, argv, "du:l:h", options, NULL)) != -1) {
        switch (c) {
        case 'd': daemon = 1; break;
        case 'u': unixsock = optarg; break;
        case 'l': logerr = optarg; break;
        case 'h': usage(stdout); break;
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

    if (unixsock) {
        int fd;

        fd = unix_listen(unixsock);
        if (fd < 0) {
            fprintf(stderr, "unix_listen(%s), err = %d\n", unixsock, err);
            return 1;
        }

        dup2(fd, STDIN_FILENO);
        dup2(fd, STDOUT_FILENO);
        close(fd);

        /* no buffer */
        setvbuf(stdin, NULL, _IONBF, 0);
        setvbuf(stdout, NULL, _IONBF, 0);
    }

    if (!bcm2835_init())
        return 1;

    /* main loop */
    while (fgets(buffer, sizeof(buffer), stdin) != NULL) {
        char *str, *cmdexec, *saveptr;

        for (str = buffer; cmdexec = strtok_r(str, ";", &saveptr); str = NULL) {

            err = module_cmdexec(cmdexec);
            if (err != 0)
                fprintf(stderr, "module_cmdexec(%s), err = %d\n", cmdexec, err);
        }
    }

    bcm2835_close();

    return 0;
}
