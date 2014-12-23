# Makefile for catnet

include ../rules.mak

CFLAGS += -g -I../lib -I../libbcm2835 -I../libevent/include
LDFLAGS += -L ../lib -llua -ldl -lm
LIBS =

PROGS = raspd

SRCS_raspd = raspd.c module.c event.c luaenv.c \
	gpiolib.c gpio.c pwm.c l298n.c ultrasonic.c \
	tankcontrol.c motor.c automatic.c

$(foreach prog, $(PROGS), $(eval OBJS_$(prog) = $(SRCS_$(prog):.c=.o)))

.PHONY: all clean

all: $(PROGS)

raspd: $(OBJS_raspd) ../lib/libraspberry.a ../libbcm2835/libbcm2835.a ../libevent/libevent.a
	$(call quiet-command, $(LD) $(LDFLAGS) -o $@ $^ $(LIBS), "  LD    $(TARGET_DIR)$@")

ifneq ($(MAKECMDGOALS), clean)
$(foreach prog, $(PROGS), $(eval -include $(SRCS_$(prog):.c=.d)))
endif

clean:
	rm -f $(SRCS_raspd:.c=.d) $(OBJS_raspd) $(PROGS)
