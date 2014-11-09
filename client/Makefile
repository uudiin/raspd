# Makefile for client

include ../rules.mak
LD = ld
CFLAGS += -g -I../lib
LDFLAGS = -L ../lib -shared -fpic
LIBS =

PROGS = client.so

SRCS_client = client.c

$(foreach prog, $(PROGS), $(eval OBJS_$(prog) = $(SRCS_$(prog):%.c=%.o)))

.PHONY: all clean

all: $(PROGS)

client.so: $(OBJS_client) ../lib/libraspberry.a
	$(call quiet-command, $(LD) $(LDFLAGS) -o $@ $^ $(LIBS), "  LD    $(TARGET_DIR)$@")

ifneq ($(MAKECMDGOALS), clean)
$(foreach prog, $(PROGS), $(eval -include $(SRCS_$(prog):.c=.d)))
endif

clean:
	rm -f *.d $(OBJS_client) $(PROGS)
