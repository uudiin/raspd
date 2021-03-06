#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <assert.h>
#include <dirent.h>
#include <unistd.h>
#include <fcntl.h>
#include <pthread.h>
#include <poll.h>
#include <event2/event.h>

#include <xmalloc.h>
#include <unix.h>
#include <bcm2835.h>
#include <sock.h>

#include "module.h"
#include "gpiolib.h"
#include "event.h"

#define NR_GPIOS    54
#define SYSFS_GPIO_DIR  "/sys/class/gpio/"

#define DEFAULT_GPIO_BASE       (256 - 54)  /* 202 */
static int GPIO_BASE = DEFAULT_GPIO_BASE;

#define GPIO2EXPORT(x)  ((x) + GPIO_BASE)

static char *edge_str[4] = {
    "none",
    "rising",
    "falling",
    "both"
};

static int fd_export = -1;
static int fd_unexport = -1;

static int gpio_sysfs_open(unsigned int gpio, const char *file)
{
    char buf[128];
    int len;
    int fd;

    if (gpio >= NR_GPIOS)
        return -ERANGE;

    len = snprintf(buf, sizeof(buf), SYSFS_GPIO_DIR "gpio%d/%s",
                            GPIO2EXPORT(gpio), file);
    if (len <= 0)
        return -EINVAL;

    fd = open(buf, O_RDWR);
    if (fd < 0) {
        perror(buf);
        return -EIO;
    }
    return fd;
}

#define open_active_low(x)  gpio_sysfs_open((x), "active_low")
#define open_direction(x)   gpio_sysfs_open((x), "direction")
#define open_edge(x)        gpio_sysfs_open((x), "edge")
#define open_value(x)       gpio_sysfs_open((x), "value")

static int gpio_export_unexport(unsigned int gpio, int unexport)
{
    char buf[128];
    int len;
    int err;

    if (fd_export < 0 || fd_unexport < 0)
        return -EPERM;
    if (gpio >= NR_GPIOS)
        return -ERANGE;

    /* in case the gpio already exported or not */
    len = snprintf(buf, sizeof(buf), SYSFS_GPIO_DIR "gpio%d/edge",
                    GPIO2EXPORT(gpio));
    if (len <= 0)
        return -EINVAL;

    err = access(buf, W_OK);
    if (unexport && err < 0)
        return EINVAL;
    else if (!unexport && err == 0)
        return EEXIST;

    len = snprintf(buf, sizeof(buf), "%d", GPIO2EXPORT(gpio));
    if (len <= 0)
        return -EINVAL;

    if (write(unexport ? fd_unexport : fd_export, buf, len) < 0) {
        perror("write (un)export");
        return -EIO;
    }
    return 0;
}

#define gpio_export(x)    gpio_export_unexport((x), 0)
#define gpio_unexport(x)  gpio_export_unexport((x), 1)

static int gpio_set_edge(unsigned int gpio, enum trigger_edge edge)
{
    int fd;
    int err;

    fd = open_edge(gpio);
    if (fd < 0) {
        perror("open_edge");
        return fd;
    }

    err = write(fd, edge_str[edge], strlen(edge_str[edge]));
    close(fd);
    if (err < 0) {
        perror("write edge");
        return -1;
    }
    return 0;
}

int bcm2835_gpio_poll(unsigned int pin,
        enum trigger_edge edge, int timeout, int *valuep)
{
    struct pollfd pfd;
    int fd;
    char buf[8];
    int err;

    do {
        if ((err = gpio_export(pin)) < 0)
            break;
        bcm2835_gpio_fsel(pin, BCM2835_GPIO_FSEL_INPT);
        if ((err = gpio_set_edge(pin, edge)) < 0)
            break;
        err = -EIO;
        if ((fd = open_value(pin)) < 0)
            break;

        err = -EPERM;
        if (read(fd, buf, sizeof(buf)) < 0)
            break;
        pfd.fd = fd;
        pfd.events = POLLPRI;

        while (1) {
            int ready;

            err = -EBADF;
            ready = poll(&pfd, 1, timeout);
            if (ready < 0) {
                if (errno == EINTR)
                    continue;
                else
                    break;
            } else if (ready > 0 && !(pfd.revents & POLLPRI)) {
                continue;
            }

            if (ready && valuep)
                *valuep = (int)bcm2835_gpio_lev(pin);

            close(fd);

            err = ready;
            break;
        }
    } while (0);

    return err;
}

int bcm2835_gpio_signal(unsigned int pin, enum trigger_edge edge,
                event_callback_fn cb, void *opaque, struct event **ev)
{
    char buf[8];
    struct event *evt;
    int fd;
    int err;

    do {
        if ((err = gpio_export(pin)) < 0)
            break;
        bcm2835_gpio_fsel(pin, BCM2835_GPIO_FSEL_INPT);
        if ((err = gpio_set_edge(pin, edge)) < 0)
            break;
        err = -EIO;
        if ((fd = open_value(pin)) < 0)
            break;

        /* FIXME  unblock it ? */
        unblock_socket(fd);
        /*
         * FIXME  need ?
         */
        err = -EPERM;
        if (read(fd, buf, sizeof(buf)) < 0)
            break;

        err = eventfd_add(fd, EV_PRI | EV_ET | EV_PERSIST,
                                    NULL, cb, opaque, &evt);
        if (err < 0) {
            close(fd);
            break;
        }

        if (ev)
            *ev = evt;

        err = 0;
    } while (0);

    return err;
}

static int get_gpio_base(void)
{
    struct dirent *dirp;
    DIR *dp;
    int fd;
    char basefile[256] = SYSFS_GPIO_DIR;
    char base[32] = { 0 };

    if ((dp = opendir(SYSFS_GPIO_DIR)) == NULL)
        return -ENOENT;
    while ((dirp = readdir(dp)) != NULL) {
        if (strncmp(dirp->d_name, "gpiochip", 8) != 0)
            continue;
        break;
    }

    closedir(dp);
    if (dirp == NULL)
        return -ENFILE;

    strcat(basefile, dirp->d_name);
    strcat(basefile, "/base");
    if ((fd = open(basefile, O_RDONLY)) >= 0) {
        read(fd, base, sizeof(base) - 1);
        GPIO_BASE = atoi(base);
        close(fd);
        return 0;
    }

    return -ENODEV;
}

void gpiolib_init(void)
{
    fd_export = open(SYSFS_GPIO_DIR "export", O_WRONLY);
    fd_unexport = open(SYSFS_GPIO_DIR "unexport", O_WRONLY);
    if (fd_export == -1 || fd_unexport == -1)
        perror("open (un)export");
    if (get_gpio_base() < 0)
        perror("get_gpio_base()");
}

void gpiolib_exit(void)
{
    if (fd_export != -1)
        close(fd_export);
    if (fd_unexport != -1)
        close(fd_unexport);
}
