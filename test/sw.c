#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <getopt.h>
#include <time.h>
#include <event2/event.h>

#include <xmalloc.h>
#include <bcm2835.h>

#define PRIORITY_MAXIMUM    10

static struct event_base *base;
static int s_switch = 0;

#define INVALID_PIN(x)      ((x) < 0 || (x) >= NR_BCM2835_GPIO)

struct blink {
    double len;
    int ctrl;
    int count;
    int counted;
    int interval;
    struct event *timer;
    struct event *ev_done;
    int gpio;
};

int rasp_event_init(void)
{
    base = event_base_new();
    if (base == NULL)
        return -ENOMEM;

    event_base_priority_init(base, PRIORITY_MAXIMUM);
    return 0;
}

void rasp_event_exit(void)
{
    /* FIXME */
    event_base_free(base);
}

void eventfd_del(struct event *ev)
{
    if (ev) {
        event_del(ev);
        event_free(ev);
    }
}

int eventfd_add(int fd, short flags, struct timeval *timeout,
        event_callback_fn cb, void *opaque, struct event **eventp)
{
    struct event *ev;

    ev = event_new(base, fd, flags, cb, opaque);
    if (ev == NULL)
        return -ENOMEM;

    if (event_add(ev, timeout) < 0) {
        event_free(ev);
        return -EIO;
    }

    if (eventp)
        *eventp = ev;

    return 0;
}

int register_timer(short flags, struct timeval *timeout,
        event_callback_fn cb, void *opaque, struct event **eventp)
{
    return eventfd_add(-1, flags, timeout, cb, opaque, eventp);
}

static void trig_done(int fd, short what, void *arg)
{
    struct blink *bl = arg;
    bcm2835_gpio_write(bl->gpio, LOW);
    /* FIXME  need ? */
    /*evtimer_del(bl->ev_done);*/
}

static void cb_timer(int fd, short what, void *arg)
{
    struct blink *bl = arg;
    struct timeval tv;
    int pin;

    if (bl->count != -1 && bl->counted++ >= bl->count) {
        eventfd_del(bl->timer);
        eventfd_del(bl->ev_done);
        free(bl);
        return;
    }

    pin = bl->gpio;

    bcm2835_gpio_write(pin, HIGH);
    /*
    usleep(bl->len);
    bcm2835_gpio_write(pin, LOW);
    */
 
    if (s_switch != 0) {
        bl->ctrl = (bl->ctrl == 0) ? 200 : 0;
        s_switch = 0;
    }
    printf("cb_timer pin = %d, ctrl = %d\n", pin, bl->ctrl);

    bl->len = 1000 * (0.9 + bl->ctrl * (2.1 - 0.9) / 200.0);
    tv.tv_sec = (int)bl->len / 1000000;
    tv.tv_usec = (int)bl->len % 1000000;
    evtimer_add(bl->ev_done, &tv);
}

static void cb_read(int fd, short what, void *arg)
{
    struct event *ev = *(void **)arg;

    char buffer[512];
    int size;

    size = read(fd, buffer, sizeof(buffer));
    s_switch = 1;
    printf("input: %s", buffer);
}

/*
 * usage:
 *   skywalker --ctrlnum [0-200] --count [num] --interval 20 18
 */
int main(int argc, char *argv[])
{
    static struct option options[] = {
        { "ctrlnum",  required_argument, NULL, 'c' },
        { "count",    required_argument, NULL, 'n' },
        { "interval", required_argument, NULL, 't' },
        { 0, 0, 0, 0 }
    };
    int pin;
    int c;
    int count = 200000;
    int interval = 20;  /* millisecond */
    int ctrl = 0;
    struct blink *bl;
    struct timeval timeout;
    struct event *ev_stdin;
    int err = -EFAULT;

    while ((c = getopt_long(argc, argv, "c:n:t:", options, NULL)) != -1) {
        switch (c) {
        case 'c': ctrl = atoi(optarg); break;
        case 'n': count = atoi(optarg); break;
        case 't': interval = atoi(optarg); break;
        default:
            return 1;
        }
    }

    if (optind >= argc)
        return 1;

    if (!bcm2835_init())
        return 1;
    rasp_event_init();
    pin = atoi(argv[optind]);
    bcm2835_gpio_fsel(pin, BCM2835_GPIO_FSEL_OUTP);

    do {
        bl = xmalloc(sizeof(*bl));
        memset(bl, 0, sizeof(*bl));
        bl->len = 1000 * (0.9 + ctrl * (2.1 - 0.9) / 200.0);
        bl->ctrl = ctrl;
        bl->count = count;
        bl->interval = interval;
        bl->gpio = pin;
        timeout.tv_sec = interval / 1000;
        timeout.tv_usec = (interval % 1000) * 1000;

        err = -ENOMEM;
        bl->ev_done = evtimer_new(base, trig_done, bl);
        if (bl->ev_done == NULL)
            break;

        err = eventfd_add(STDIN_FILENO, EV_READ | EV_PERSIST,
                        NULL, cb_read, (void *)&ev_stdin, &ev_stdin);
        if (err < 0) {
            fprintf(stderr, "eventfd_add(STDIN_FILENO), err = %d\n", err);
            return 1;
        }

        err = -EIO;
        if (register_timer(EV_PERSIST, &timeout, cb_timer, bl, &bl->timer) < 0)
            break;

        event_base_dispatch(base);

        rasp_event_exit();

        err = 0;
    } while (0);

    if (err != 0) {
        if (bl) {
            if (bl->ev_done)
                event_free(bl->ev_done);
            if (bl->timer)
                event_free(bl->timer);
            free(bl);
        }

        return 1;
    }

    bcm2835_close();
    return 0;
}
