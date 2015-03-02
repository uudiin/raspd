#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <getopt.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/socket.h>
#include <event2/event.h>

#include <xmalloc.h>
#include <unix.h>
#include <sock.h>

#include <bcm2835.h>

#include "module.h"
#include "event.h"
#include "luaenv.h"
#include "gpiolib.h"
#include "softpwm.h"
#include "config.h"

#define BUF_SIZE    1024

struct client_info {
    int fd;
    struct event *ev;
};

static int read_cmdexec(int fd, int wfd)
{
    char buffer[BUF_SIZE];
    int size;

    size = read(fd, buffer, sizeof(buffer));
    if (size > 0) {
        char *str, *cmdexec, *saveptr;

        if (size == sizeof(buffer))
            buffer[BUF_SIZE - 1] = '\0';
        else
            buffer[size] = '\0';

        for (str = buffer; cmdexec = strtok_r(str, ";", &saveptr); str = NULL) {
            int retval;
            char errmsg[32];
            size_t len;

            retval = module_cmdexec(wfd, cmdexec);
            /* must reply */
            if (retval == 0) {
                write(wfd, "OK\n", 3);
            } else {
                len = snprintf(errmsg, sizeof(errmsg), "ERR %d\n", retval);
                write(wfd, errmsg, len);
            }
        }
    }
    return size;
}

static void cb_read(int fd, short what, void *arg)
{
    struct event *ev = *(void **)arg;

    if (read_cmdexec(fd, STDOUT_FILENO) < 0)
        eventfd_del(ev);
}

static void cb_recv(int fd, short what, void *arg)
{
    struct client_info *info = arg;

    if (read_cmdexec(fd, fd) < 0) {
        eventfd_del(info->ev);
        free(info);
    }
}

static void cb_listen(int fd, short what, void *arg)
{
    union sockaddr_u remoteaddr;
    socklen_t ss_len;
    struct client_info *info;
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

    info = xmalloc(sizeof(*info));
    info->fd = fd_cli;

    err = eventfd_add(fd_cli, EV_READ | EV_PERSIST,
                NULL, cb_recv, info, &info->ev);
    if (err < 0) {
        fprintf(stderr, "eventfd_add(), err = %d\n", err);
        free(info);
        close(fd_cli);
        return;
    }
}

static int modexec_early_init(struct module *m, void *opaque)
{
    if (m->early_init)
        return m->early_init();
    return ENOENT;
}

static int modexec_init(struct module *m, void *opaque)
{
    if (m->init)
        return m->init();
    return ENOENT;
}

static int modexec_exit(struct module *m, void *opaque)
{
    if (m->exit)
        m->exit();
    return 0;
}

/* shutdown -- its super important to reset the DMA before quitting */
static void terminate(int dummy)
{
    /* uninitialize all modules */
    foreach_module(modexec_exit, NULL);

    /* TODO */
    softpwm_exit();

    luaenv_exit();
    gpiolib_exit();
    bcm2835_close();
    rasp_event_exit();
    exit(1);
}

/*
 * Catch all signals possible - it is vital we kill the DMA engine
 * on process exit!
 */
static void setup_sighandlers(void)
{
    struct sigaction sa;
    int i;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = terminate;
    for (i = 0; i < 64; i++)
        sigaction(i, &sa, NULL);
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
        "  -c, --lua-config <file>   specify the lua config file\n"
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
        { "lua-config",  required_argument, NULL, 'c' },
        { "help",        no_argument,       NULL, 'h' },
        { 0, 0, 0, 0 }
    };
    int c;
    int daemon = 0;
    char *unixlisten = NULL;
    char *logerr = NULL;
    char *lua_conf = DEFAULT_LUA_CONFIG;
    long listen_port = 0;
    union sockaddr_u addr;
    int fd = -1;
    int err;

    while ((c = getopt_long(argc, argv, "dl:u:e:c:h", options, NULL)) != -1) {
        switch (c) {
        case 'd': daemon = 1; break;
        case 'l': listen_port = strtol(optarg, NULL, 10); break;
        case 'u': unixlisten = optarg; break;
        case 'e': logerr = optarg; break;
        case 'c': lua_conf = optarg; break;
        case 'h': usage(stdout); break;
        default:
            usage(stderr);
            break;
        }
    }

    /* set signel handlers */
    setup_sighandlers();

    /*
     * luaenv
     * config
     * daemon
     * pidfile
     * event
     */

    /* initialize luaenv */
    if ((err = luaenv_init()) < 0) {
        fprintf(stderr, "luaenv_init(), err = %d\n", err);
        return 1;
    }
    if ((err = luaenv_run_file(lua_conf)) < 0)
        fprintf(stderr, "luaenv_run_file(%s), err = %d\n", lua_conf, err);

    /* if run as daemon ? */
    if (daemon) {
        err = daemonize("raspd");
        if (err < 0) {
            fprintf(stderr, "daemonize(), err = %d\n", err);
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

    /* initialize bcm2835 */
    if (!bcm2835_init()) {
        fprintf(stderr, "bcm2835_init() error\n");
        return 1;
    }
    /* init gpiolib */
    gpiolib_init();

    /* early init */
    if ((err = foreach_module(modexec_early_init, NULL)) < 0)
        return 1;

    /* set realtime task */
    if ((err = sched_realtime()) < 0)
    	fprintf(stderr, "sched_realtime(), err = %d\n", err);

    /* initialize event base */
    if (rasp_event_init() < 0) {
        fprintf(stderr, "event_init(), err = %d\n", err);
        return 1;
    }

    /*
     * run the lua file
     * initialize devtree
     */
    do {
        const char *lua_file = NULL;
        luaenv_getconf_str("_G", "devres", &lua_file);
        if (lua_file) {
            if ((err = luaenv_run_file(lua_file)) < 0)
                fprintf(stderr, "luaenv_run_fle(%s), err = %d\n", lua_file, err);
            luaenv_pop(1);
        }
    } while (0);

    /* if not daemon, get data from stdin */
    if (!daemon) {
        static struct event *ev_stdin;
        err = eventfd_add(STDIN_FILENO, EV_READ | EV_PERSIST,
                        NULL, cb_read, (void *)&ev_stdin, &ev_stdin);
        if (err < 0) {
            fprintf(stderr, "eventfd_add(STDIN_FILENO), err = %d\n", err);
            return 1;
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

        fd = do_listen(SOCK_STREAM, IPPROTO_TCP, &addr);
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

        fd = do_listen(SOCK_STREAM, IPPROTO_TCP, &addr);
        if (fd < 0) {
            perror("do_listen()\n");
            return 1;
        }
    }

    if (fd >= 0) {
        static struct event *ev_listen;
        unblock_socket(fd);
        err = eventfd_add(fd, EV_READ | EV_PERSIST,
                    NULL, cb_listen,(void *)&ev_listen, &ev_listen);
        if (err < 0) {
            perror("eventfd_add()\n");
            return 1;
        }
    }

    /* initialize all modules */
    if ((err = foreach_module(modexec_init, NULL)) < 0)
        return 1;

    /* main loop */
    rasp_event_loop();

    if (fd != -1)
        close(fd);

    /* uninitialize all modules */
    foreach_module(modexec_exit, NULL);

    luaenv_exit();
    gpiolib_exit();
    bcm2835_close();
    rasp_event_exit();

    return 0;
}
