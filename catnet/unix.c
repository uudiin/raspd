#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <fcntl.h>
#include <unistd.h>
#include <signal.h>
#include <syslog.h>

#include "catnet.h"
#include "xmalloc.h"

static int write_loop(int fd, char *buf, size_t size)
{
    char *p;
    int n;

    p = buf;
    while (p - buf < size) {
        n = write(fd, p, size - (p - buf));
        if (n == -1) {
            if (errno == EINTR)
                continue;
            else
                break;
        }
        p += n;
    }
    return p - buf;
}

int lockfile(int fd)
{
    struct flock fl;
    fl.l_type = F_WRLCK;
    fl.l_start = 0;
    fl.l_whence = SEEK_SET;
    fl.l_len = 0;
    return fcntl(fd, F_SETLK, &fl);
}

int daemonize(const char *cmd)
{
    int i, fd0, fd1, fd2;
    pid_t pid;
    struct rlimit rl;
    struct sigaction sa;

    umask(0);

    if (getrlimit(RLIMIT_NOFILE, &rl) < 0)
        return -EIO;

    /* become a session leader to lose controlling TTY */
    if ((pid = fork()) < 0)
        return -EFAULT;
    else if (pid > 0)
        exit(0);

    setsid();

    /* ensure future opens won't allocate controlling TTYs */
    sa.sa_handler = SIG_IGN;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    if (sigaction(SIGHUP, &sa, NULL) < 0)
        return -EIO;

    if ((pid = fork()) < 0)
        return -EFAULT;
    else if (pid > 0)
        exit(0);

    /* we won't prevent file system from being unmounted */
    if (chdir("/") < 0)
        return -EFAULT;

    if (rl.rlim_max == RLIM_INFINITY)
        rl.rlim_max = 1024;
    for (i = 0; i < rl.rlim_max; i++)
        close(i);

    fd0 = open("/dev/null", O_RDWR);
    fd1 = dup(0);
    fd2 = dup(0);

    openlog(cmd, LOG_CONS, LOG_DAEMON);
    if (fd0 != 0 || fd1 != 1 || fd2 != 2) {
        syslog(LOG_ERR, "unexpected file descriptors %d %d %d", fd0, fd1, fd2);
        return -EFAULT;
    }

    return 0;
}

/*
 * Split a command line into an array suitable for handing to execv.
 *
 * A note on syntax: words are split on whitespace and '\' escapes characters.
 * '\\' will show up as '\' and '\ ' will leave a space, combining two
 * words.  Examples:
 * "ncat\ experiment -l -k" will be parsed as the following tokens:
 * "ncat experiment", "-l", "-k".
 * "ncat\\ -l -k" will be parsed as "ncat\", "-l", "-k"
 * See the test program, test/test-cmdline-split to see additional cases.
 */
char **cmdline_split(const char *cmdexec)
{
    const char *ptr;
    char *cur_arg, **cmd_args;
    int max_tokens = 0, arg_idx = 0, ptr_idx = 0;

    /* Figure out the maximum number of tokens needed */
    ptr = cmdexec;
    while (*ptr) {
        /* Find the start of the token */
        while (*ptr && isspace((int)(unsigned char)*ptr))
            ptr++;
        if (!*ptr)
            break;
        max_tokens++;
        /* Find the start of the whitespace again */
        while (*ptr && !isspace((int)(unsigned char)*ptr))
            ptr++;
    }

    /* The line is not empty so we've got something to deal with */
    cmd_args = (char **)xmalloc(sizeof(char *) * (max_tokens + 1));
    cur_arg = (char *)calloc(sizeof(char), strlen(cmdexec));

    /* Get and copy the tokens */
    ptr = cmdexec;
    while (*ptr) {
        while (*ptr && isspace((int)(unsigned char)*ptr))
            ptr++;
        if (!*ptr)
            break;

        while (*ptr && !isspace((int)(unsigned char)*ptr)) {
            if (*ptr == '\\') {
                ptr++;
                if (!*ptr)
                    break;

                cur_arg[ptr_idx] = *ptr;
                ptr_idx++;
                ptr++;

                if (*(ptr - 1) != '\\') {
                    while (*ptr && isspace((int)(unsigned char)*ptr))
                        ptr++;
                }
            } else {
                cur_arg[ptr_idx] = *ptr;
                ptr_idx++;
                ptr++;
            }
        }
        cur_arg[ptr_idx] = '\0';

        cmd_args[arg_idx] = strdup(cur_arg);
        cur_arg[0] = '\0';
        ptr_idx = 0;
        arg_idx++;
    }

    cmd_args[arg_idx] = NULL;

    /* Clean up */
    free(cur_arg);

    return cmd_args;
}

int netexec(int fd, const char *cmdexec)
{
    int child_stdin[2];
    int child_stdout[2];
    int pid;
    char buffer[DEFAULT_TCP_BUFLEN];
    int maxfd;

    if (pipe(child_stdin) == -1 || pipe(child_stdout) == -1)
        return -1;

    pid = fork();
    if (pid == -1)
        return -1;

    if (pid == 0) {
        /* child */
        char **cmdargs;

        close(child_stdin[1]);
        close(child_stdout[0]);

        signal(SIGPIPE, SIG_DFL);

        dup2(child_stdin[0], STDIN_FILENO);
        dup2(child_stdout[1], STDOUT_FILENO);

        cmdargs = cmdline_split(cmdexec);
        execv(cmdargs[0], cmdargs);

        return -1;
    }

    /* parent */
    close(child_stdin[0]);
    close(child_stdout[1]);

    maxfd = child_stdout[0];
    if (fd > maxfd)
        maxfd = fd;

    while (1) {
        fd_set fds;
        int n;

        FD_ZERO(&fds);
        FD_SET(fd, &fds);
        FD_SET(child_stdout[0], &fds);
        n = select(maxfd + 1, &fds, NULL, NULL, NULL);
        if (n == -1) {
            if (errno == EINTR)
                continue;
            else
                break;
        }

        if (FD_ISSET(fd, &fds)) {
            n = recv(fd, buffer, sizeof(buffer), 0);
            if (n <= 0)
                break;
            write_loop(child_stdin[1], buffer, n);
        }

        if (FD_ISSET(child_stdout[0], &fds)) {
            n = read(child_stdout[0], buffer, sizeof(buffer));
            if (n <= 0)
                break;
            send(fd, buffer, n, 0);
        }
    }

    close(fd);
    exit(0);

    return 0;
}

int netrun(int fd, const char *cmdexec)
{
    int pid;

    pid = fork();
    if (pid == 0)
        netexec(fd, cmdexec);

    close(fd);

    return pid;
}
