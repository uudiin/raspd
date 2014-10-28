# Makefile for catnet

include ../rules.mak

CFLAGS += -g -I../lib
LDFLAGS += -L ../lib
LIBS =

PROGS = catnet

SRCS_catnet = catnet.c listen.c connect.c

$(foreach prog, $(PROGS), $(eval OBJS_$(prog) = $(SRCS_$(prog):%.c=%.o)))

.PHONY: all clean

all: $(PROGS)

catnet: $(OBJS_catnet) ../lib/libraspberry.a
	$(call quiet-command, $(LD) $(LDFLAGS) -o $@ $^ $(LIBS), "  LD    $@")

ifneq ($(MAKECMDGOALS), clean)
$(foreach prog, $(PROGS), $(eval -include $(SRCS_$(prog):.c=.d)))
endif

clean:
	rm -f *.d $(OBJS_catnet) $(PROGS)
