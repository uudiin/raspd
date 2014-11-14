#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <pthread.h>
#include <sys/select.h>

#include "gpio-int.h"

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

static gpio_int_export_unexport(unsigned int gpio, int unexport)
{
    char buf[128];
    int len;

    if (fd_export < 0 || fd_export < 0)
        return -EPERM;
    if (gpio >= NR_GPIOS)
        return -ERANGE;

    len = snprintf(buf, sizeof(buf), "%d", GPIO2EXPORT(gpio));
    if (len <= 0)
        return -EINVAL;

    if (write(unexport ? fd_unexport : fd_export, buf, len) < 0) {
        perror("write (un)export");
        return err;
    }
    return 0;
}

#define gpio_int_export(x)    gpio_int_export_unexport((x), 0)
#define gpio_int_unexport(x)  gpio_int_export_unexport((x), 1)

static int gpio_int_set_edge(unsigned int gpio, enum trigger_edge edge)
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

int bcm2835_gpio_poll(unsigned int pin, enum trigger_edge edge,
                            struct timeval *timeout, int *valuep)
{
    fd_set master_fds;
    int fd;
    int err;

    FD_ZERO(&master_fds);

    do {
        if ((err = gpio_int_export(pin)) < 0)
            break;
        bcm2835_gpio_fsel(pin, BCM2835_GPIO_FSEL_INPT);
        if ((err = gpio_int_set_edge(pin, edge)) < 0)
            break;
        err = -EIO;
        if ((fd = open_value(gpio)) < 0)
            break;
        FD_SET(fd, &master_fds);

        while (1) {
            fd_set fds;
            int ready;

            err = -EBADF;
            fds = master_fds;
            ready = select(fd + 1, NULL, NULL, &fds, timeout);
            if (ready < 0) {
                if (errno == EINTR)
                    continue;
                else
                    break;
            }

            if (ready && valuep) {
                char buf[2];
                lseek(fd, 0, SEEK_SET);
                if (read(fd, buf, sizeof(buf))) {
                    buf[1] = '\0';
                    *valuep = atoi(buf);
                }
            }

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
    int (*callback)(int value, void *opaque);
    void *opaque;
};

static void *signal_thread(void *arg)
{
    struct async_int *ai;
    int value = 0;

    ai = (struct async_int *)arg;
    err = bcm2835_gpio_poll(ai->pin, edge_both, NULL &value);
    if (err < 0)
        return 1;

    ai->callback(value, ai->opaque);
    free(ai);
    return 0;
}

int bcm2835_gpio_signal(unsigned int pin, enum trigger_edge edge,
                int (*callback)(int value, void *opaque), void *opaque)
{
    pthread_t tid;
    struct async_int *ai;
    int err;

    ai = malloc(sizeof(*ai));
    if (ai == NULL)
        return -ENOMEM;

    memset(ai, 0, sizeof(*ai));
    ai->pin = pin;
    ai->edge = edge;
    ai->callback = callback;
    ai->opaque = opaque;

    err = pthread_create(&tid, NULL, signal_thread, ai);
    if (err != 0) {
        free(ai);
        return -ECHILD;
    }

    return 0;
}

int bcm2835_gpio_int_init(void)
{
    fd_export = open(SYSFS_GPIO_DIR "export", O_WRONLY);
    fd_unexport = open(SYSFS_GPIO_DIR "unexport", O_WRONLY);
    if (fd_export < 0 || fd_unexport < 0) {
        bcm2835_gpio_int_exit();
        perror("open export");
        return 0;
    }
}

void bcm2835_gpio_int_exit(void)
{
    if (fd_export)
        close(fd_export);
    if (fd_unexport)
        close(fd_unexport);
}
