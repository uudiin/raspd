# Makefile for client

include ../rules.mak
CFLAGS += -g -I../lib -I../libevent/include -fPIC
LDFLAGS = -L ../lib -pthread
LIBS =

PROGS = libclient.a test

SRCS_client = client.c l298n.c
OBJS_client = client.o l298n.o
OBJS_test = test.o

.PHONY: all clean

all: $(PROGS)

libclient.a: $(OBJS_client)

test: $(OBJS_test) libclient.a ../lib/libraspberry.a ../libevent/libevent.a
	$(call quiet-command, $(CC) $(LDFLAGS) -o $@ $^ $(LIBS), "  CC    $(TARGET_DIR)$@")

ifneq ($(MAKECMDGOALS), clean)
$(foreach prog, $(PROGS), $(eval -include $(SRCS_$(prog):.c=.d)))
endif

clean:
	rm -f *.d $(OBJS_client) $(OBJS_test) $(PROGS)
