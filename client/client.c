#include <stdio.h>
#include <errno.h>
#include <pthread.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <sys/time.h>
#include <unistd.h>

#include <sock.h>
#include <event2/event.h>

#define PRIORITY_MAXIMUM    10

static int fd = 0;
static struct event_base *base;

static void cb_recv(int fd, short what, void *arg)
{
	void (*cb)(char *buf, int buflen) = arg;
	char buf[1024] = "\0";
	read(fd, buf, 1024);
	if (cb != NULL) {
		cb(buf, strlen(buf));
	}
	perror(buf);
}

static int client_event_init(void)
{
    base = event_base_new();
    if (base == NULL)
        return -ENOMEM;

    event_base_priority_init(base, PRIORITY_MAXIMUM);
    return 0;
}

static int client_eventfd_add(int fd, short flags, struct timeval *timeout,
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

static void *thread_func(void *arg)
{
	int err;
	static struct event *ev_recv;

	err = client_event_init();
	if (err < 0) {
		perror("rasp_event_init()\n");
		return NULL;
	}

	unblock_socket(fd);
	err = client_eventfd_add(fd, EV_READ | EV_PERSIST,
		NULL, cb_recv, arg, &ev_recv);
	if (err < 0) {
		perror("eventfd_add()\n");
		return NULL;
	}

	event_base_dispatch(base);
	return NULL;
}

int client_msg_dispatch(void (*cb)(char *buf, int buflen))
{
	pthread_t thread;
	return pthread_create(&thread, NULL, thread_func, cb);
}

int client_connect(const char *hostname, unsigned short portno)
{
	union sockaddr_u addr;
	size_t ss_len;
	int err;

	ss_len = sizeof(addr);
	err = resolve(hostname, portno, &addr.storage, &ss_len, AF_INET, 0);
	if (err < 0) {
		return err;
	}

	fd = do_connect(SOCK_STREAM, &addr);
	if (fd < 0) {
		return -1;
	}

	return err;
}

int client_send_cmd(char *buf, int bufsize)
{
	return send(fd, buf, bufsize, 0);
}
