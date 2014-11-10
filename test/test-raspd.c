#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>

int main(int argc, char *argv[])
{
    static struct option options[] = {
        { "unix",    required_argument, NULL, 'u' },
        { 0, 0, 0, 0 }
    };
    int c;
    char *unix = NULL;
    int fd;
    union sockaddr_u addr;
    socklen_t len;
    char buffer[BUF_SIZE];
    int err;

    while ((c = getopt_long(argc, argv, "du:", options, NULL)) != -1) {
        switch (c) {
        case 'u': unix = optarg; break;
        }
    }

    if (unix == NULL) {
        fprintf(stderr, "must be set the unix socket file");
        return 1;
    }

    if ((fd = socket(AF_UNIX, SOCK_STREAM, 0)) < 0)
        return 1;

    if (strlen(unix) >= sizeof(addr.un.sun_path))
        return 1;

    memset(&addr, 0, sizeof(addr));
    addr.un.sun_family = AF_UNIX;
    strncpy(addr.un.sun_path, unix, sizeof(addr.un.sun_path));
    len = SUN_LEN(&addr.un);
    if (connect(fd, &addr.sockaddr, len) < 0)
        return 1;

    /* main loop */
    while (fgets(buffer, sizeof(buffer), stdin) != NULL) {
        size_t size = strlen(buffer);
        int retval;

        retval = write(fd, buffer, size);
        if (retval != size)
            fprintf(stderr, "send date error, err = %d\n", retval);

        retval = read(fd, buffer, sizeof(buffer));
        if (retval)
            fprintf(stdout, "%s", buffer);
    }

    return 0;
}
