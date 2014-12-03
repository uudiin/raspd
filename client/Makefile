# Makefile for client

include ../rules.mak
CFLAGS += -g -I../lib -fPIC
LDFLAGS = -L ../lib
LIBS =

PROGS = libclient.a libclient.so test

SRCS_client = client.c l298n.c
OBJS_client = client.o l298n.o
OBJS_test = test.o

#$(foreach prog, $(PROGS), $(eval OBJS_$(prog) = $(SRCS_$(prog):%.c=%.o)))

.PHONY: all clean

all: $(PROGS)

libclient.so: $(OBJS_client)

libclient.a: $(OBJS_client)

test: $(OBJS_test) libclient.a ../lib/libraspberry.a
	$(call quiet-command, $(CC) $(LDFLAGS) -o $@ $^ $(LIBS), "  CC    $(TARGET_DIR)$@")

ifneq ($(MAKECMDGOALS), clean)
$(foreach prog, $(PROGS), $(eval -include $(SRCS_$(prog):.c=.d)))
endif

clean:
	rm -f *.d $(OBJS_client) $(PROGS)
