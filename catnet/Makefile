# Makefile for catnet

CC = gcc
LD = gcc
CFLAGS = -g
LDFLAGS =
LIBS =

DGFLAGS = -MMD -MP -MT $@ -MF $(*D)/$(*F).d

PROGS = catnet

SRCS_catnet = catnet.c listen.c connect.c sock.c unix.c utils.c xmalloc.c

$(foreach prog, $(PROGS), $(eval OBJS_$(prog) = $(SRCS_$(prog):%.c=%.o)))

.PHONY: all clean

all: $(PROGS)

catnet: $(OBJS_catnet)
	$(call quiet-command, $(LD) $(LDFLAGS) -o $@ $^ $(LIBS), "  LD    $@")

%.o: %.c
	$(call quiet-command, $(CC) $(CFLAGS) $(DGFLAGS) -c -o $@ $<, "  CC    $@")

ifneq ($(MAKECMDGOALS), clean)
$(foreach prog, $(PROGS), $(eval -include $(SRCS_$(prog):.c=.d)))
endif

clean:
	rm -f *.d $(OBJS_catnet) $(PROGS)

quiet-command = $(if $(V), $1, $(if $2, @echo $2 && $1, @$1))
