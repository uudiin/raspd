# Makefile for catnet

CC = gcc
CFLAGS = -g
LDFLAGS =

DGFLAGS = -MMD -MF -MT $@ -MF $(*D)/$(*F).d

PROGS = catnet

SRCS_catnet = catnet.c listen.c connect.c sock.c unix.c utils.c xmalloc.c

$(foreach prog, $(PROGS), $(eval OBJS_$(prog) = $(SRCS_$(prog):%.c=%.o)))

.PHONY: all clean

all: $(PROGS)

catnet: $(OBJS_catnet)
	$(CC) -o $@ $^ $(LDFLAGS) $(LIBS)

%.o: %.c
	$(CC) $< -c -o $@ $(CFLAGS) $(DGFLAGS)

ifneq ($(MAKECMDGOALS), clean)
$(foreach prog, $(PROGS), $(eval -include $(SRCS_$(prog):.c=.d)))
endif

clean:
	rm -f *.d $(OBJS_catnet) $(PORGS)
