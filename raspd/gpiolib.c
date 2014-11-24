#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <assert.h>
#include <unistd.h>
#include <fcntl.h>
#include <pthread.h>
#include <poll.h>
#include <event2/event.h>

#include <xmalloc.h>
#include <unix.h>
#include <bcm2835.h>

#include "module.h"
#include "gpiolib.h"

#define NR_GPIOS    54
#define SYSFS_GPIO_DIR  "/sys/class/gpio/"

#define GPIO_BASE       (256 - 54)  /* 202 */
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

struct async_int {
    unsigned int pin;
    enum trigger_edge edge;
    int (*callback)(int nr, int value, void *opaque);
    int nr;
    void *opaque;
    struct event *ev;
};

static void cb_gpiolib(int fd, short what, void *arg)
{
    struct async_int *ai = arg;
    int value;
    int err;

    value = (int)bcm2835_gpio_lev(ai->pin);
    err = ai->callback(ai->nr, value, ai->opaque);
    ai->nr++;
    if (err < 0) {
        eventfd_del(ai->ev);
        close(fd);
        free(ai);
    }
}

int bcm2835_gpio_signal(unsigned int pin, enum trigger_edge edge,
        int (*callback)(int nr, int value, void *opaque), void *opaque)
{
    char buf[8];
    struct async_int *ai;
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

        ai = xmalloc(sizeof(*ai));
        memset(ai, 0, sizeof(*ai));
        ai->pin = pin;
        ai->edge = edge;
        ai->callback = callback;
        ai->opaque = opaque;

        err = eventfd_add(fd, EV_PRI | EV_ET | EV_PERSIST,
                    NULL, cb_gpiolib, ai, &ai->ev);
        if (err < 0) {
            close(fd);
            free(ai);
            break;
        }

        err = 0;
    } while (0);

    return err;
}

static __init void gpiolib_init(void)
{
    fd_export = open(SYSFS_GPIO_DIR "export", O_WRONLY);
    fd_unexport = open(SYSFS_GPIO_DIR "unexport", O_WRONLY);
}

static __exit void gpiolib_exit(void)
{
    if (fd_export != -1)
        close(fd_export);
    if (fd_unexport != -1)
        close(fd_unexport);
}
