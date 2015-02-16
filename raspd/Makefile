# Makefile for catnet

include ../rules.mak

CFLAGS += -g -I../lib -I../libbcm2835 -I../libevent/include
LDFLAGS += -L ../lib -llua -ldl -lm
LIBS =

STATIC_LIBS = libraspd.a
PROGS = raspd

SRCS_libraspd = event.c gpiolib.c

SRCS_raspd = raspd.c module.c event.c luaenv.c softpwm.c \
	gpiolib.c gpio.c pwm.c l298n.c ultrasonic.c \
	tankcontrol.c motor.c modmisc.c inv_mpu.c pid.c pidctrl.c

DEPS_raspd = ../lib/libraspberry.a \
			 ../libbcm2835/libbcm2835.a \
			 ../libevent/libevent.a \
			 ../inv_mpu/libinv_mpu.a

OBJS_libraspd = $(SRCS_libraspd:.c=.o)

$(foreach prog, $(PROGS), $(eval OBJS_$(prog) = $(SRCS_$(prog):.c=.o)))

.PHONY: all clean

all: $(STATIC_LIBS) $(PROGS)

libraspd.a: $(OBJS_libraspd)

raspd: $(OBJS_raspd) $(DEPS_raspd)
	$(call quiet-command, $(LD) $(LDFLAGS) -o $@ $^ $(LIBS), "  LD    $(TARGET_DIR)$@")

ifneq ($(MAKECMDGOALS), clean)
$(foreach prog, $(PROGS), $(eval -include $(SRCS_$(prog):.c=.d)))
endif

clean:
	rm -f $(SRCS_libraspd:.c=.d) $(STATIC_LIBS)
	rm -f $(SRCS_raspd:.c=.d) $(OBJS_raspd) $(PROGS)
