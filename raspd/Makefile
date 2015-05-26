# Makefile for raspd

include ../rules.mak

CFLAGS += -g -I../lib -I../libbcm2835 -I../libevent/include
CFLAGS += -I../inv_mpu/core/driver/eMPL -I../inv_mpu/core/driver/include -I../inv_mpu/core/mllite
CFLAGS += -DEMPL_TARGET_BCM2835 -DLINUX -DUSE_CAL_HW_REGISTERS -DLOG_STD -DLUA_COMPAT_ALL
LDFLAGS += -L ../lib -llua -ldl -lm
LIBS =

STATIC_LIBS = libraspd.a
PROGS = raspd

SRCS_libraspd = event.c gpiolib.c

SRCS_raspd = raspd.c module.c event.c luaenv.c softpwm.c \
	gpiolib.c gpio.c pwm.c l298n.c ultrasonic.c \
	tankcontrol.c motor.c modmisc.c inv_imu.c pid.c \
	quadcopter.c

DEPS_raspd = ../lib/libraspberry.a \
			 ../libbcm2835/libbcm2835.a \
			 ../libevent/libevent.a

OBJS_libraspd = $(SRCS_libraspd:.c=.o)

$(foreach prog, $(PROGS), $(eval OBJS_$(prog) = $(SRCS_$(prog):.c=.o)))


OBJS_raspd_9250 = $(OBJS_raspd:.o=.9250o)

%.9250o: %.c
	$(call quiet-command, $(CC) $(CFLAGS) $(DGFLAGS) -c -o $@ $<, "  CC    $(TARGET_DIR)$@")

.PHONY: all clean

all: $(STATIC_LIBS) $(PROGS) raspd_9250

raspd: CFLAGS += -DMPU6050

raspd_9250: CFLAGS += -DMPU9250 -DCOMPASS_ENABLED


libraspd.a: $(OBJS_libraspd)

raspd: $(OBJS_raspd) $(DEPS_raspd) ../inv_mpu/libinv_mpu.a
	$(call quiet-command, $(LD) $(LDFLAGS) -o $@ $^ $(LIBS), "  LD    $(TARGET_DIR)$@")

raspd_9250: $(OBJS_raspd_9250) $(DEPS_raspd) ../inv_mpu/libinv_mpu_9250.a
	$(call quiet-command, $(LD) $(LDFLAGS) -o $@ $^ $(LIBS), "  LD    $(TARGET_DIR)$@")

ifneq ($(MAKECMDGOALS), clean)
$(foreach prog, $(PROGS), $(eval -include $(SRCS_$(prog):.c=.d)))
endif

clean:
	rm -f $(SRCS_libraspd:.c=.d) $(STATIC_LIBS)
	rm -f $(SRCS_raspd:.c=.d) $(OBJS_raspd) $(PROGS)
	rm -f $(OBJS_raspd:.o=.9250o) raspd_9250
