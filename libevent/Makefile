# Makefile for libevent

include ../rules.mak

CFLAGS += -Icompat -Iinclude

LIBS = libevent.a

SRCS_lib = event.c evthread.c buffer.c \
	bufferevent.c bufferevent_sock.c bufferevent_filter.c \
	bufferevent_pair.c listener.c bufferevent_ratelim.c \
	evmap.c	log.c evutil.c evutil_rand.c strlcpy.c \
	epoll.c signal.c \
	evthread_pthread.c

OBJS_lib = $(SRCS_lib:.c=.o)

.PHONY: all clean

all: $(LIBS)

$(LIBS): $(OBJS_lib)

ifneq ($(MAKECMDGOALS), clean)
-include $(SRCS_lib:.c=.d)
endif

clean:
	rm -f $(OBJS_lib) $(SRCS_lib:.c=.d) $(LIBS)
