#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <getopt.h>
#include <unistd.h>

#include <bcm2835.h>

#include <unix.h>

#include "module.h"

#define BUF_SIZE    1024

int unix_listen(const char *unix)
{
    union sockaddr_u addr, remoteaddr;
    socklen_t len = sizeof(remoteaddr);
    int fd, clifd;
    int err;

    unlink(unix);

    if (strlen(unix) >= sizeof(addr.un.sun_path))
        return -EINVAL;

    memset(&addr, 0, sizeof(addr));
    addr.un.sun_family = AF_UNIX;
    strncpy(addr.un.sun_path, unix, sizeof(addr.un.sun_path));

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

    clifd = accept(fd, &remoteaddr, &len);
    if (clifd < 0)
        return -EFAULT;

    return clifd;
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
        { 0, 0, 0, 0 }
    };
    int c;
    int daemon = 0;
    char *unix = NULL;
    char *logerr = NULL;
    char buffer[BUF_SIZE];
    int err;

    while ((c = getopt_long(argc, argv, "du:", options, NULL)) != -1) {
        switch (c) {
        case 'd': daemon = 1; break;
        case 'u': unix = optarg; break;
        case 'l': logerr = optarg; break;
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
        logfd = open(logerr, O_RDWR);
        if (logfd) {
            dup2(logfd, FILENO_STDERR);
            close(logfd);
        }
    }

    if (unix) {
        int fd;

        fd = unix_listen(unix);
        if (fd < 0) {
            fprintf(stderr, "unix_listen(%s), err = %d\n", unix, err);
            return 1;
        }

        dup2(fd, FILENO_STDIN);
        dup2(fd, FILENO_STDOUT);
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
            else
                fprintf(stdout, "OK\n");
        }
    }

    bcm2835_close();

    return 0;
}
