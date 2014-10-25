# Makefile for catnet

CC = gcc
CFLAGS = -g
LDFLAGS =
LIBS =

DGFLAGS = -MMD -MF -MT $@ -MF $(*D)/$(*F).d

PROGS = catnet

SRCS_catnet = catnet.c listen.c connect.c sock.c unix.c utils.c xmalloc.c

$(foreach prog, $(PROGS), $(eval OBJS_$(prog) = $(SRCS_$(prog):%.c=%.o)))

.PHONY: all clean

all: $(PROGS)

catnet: $(OBJS_catnet)
	$(CC) $(LDFLAGS) -o $@ $^ $(LIBS)

%.o: %.c
	$(CC) $(CFLAGS) $(DGFLAGS) -c -o $@ $<

ifneq ($(MAKECMDGOALS), clean)
$(foreach prog, $(PROGS), $(eval -include $(SRCS_$(prog):.c=.d)))
endif

clean:
	rm -f *.d $(OBJS_catnet) $(PORGS)

quiet-command = $(if $(V), $1, $(if $2, @echo $2 && $1, @$1))
