# Makefile for mpu motion driver

include ../rules.mak

INC_PATHS = core/driver/include \
			core/driver/eMPL \
			core/eMPL-hal \
			core/mllite \
			core/mpl \
			core/driver \
			../libbcm2835 \
			../lib \
			../libevent/include \

INCLUDES = $(addprefix -I, $(INC_PATHS))

CFLAGS += $(INCLUDES)
#CFLAGS += -DEMPL_TARGET_BCM2835 -DMPU9250 -DCOMPASS_ENABLED -DLINUX -DUSE_CAL_HW_REGISTERS -DLOG_STD
CFLAGS += -DEMPL_TARGET_BCM2835 -DMPU6050 -DLINUX -DUSE_CAL_HW_REGISTERS -DLOG_STD
LDFLAGS += -lm
LIBS =

STATIC_LIBS = libinv_mpu.a
PROGS = invmpu_dmp

SRCS_libinv_mpu = core/driver/eMPL/inv_mpu.c \
				  core/driver/eMPL/inv_mpu_dmp_motion_driver.c \
				  core/mllite/ml_math_func.c

SRCS_mpu_test = src/main.c \
			   core/driver/log_std.c \
			   core/eMPL-hal/eMPL_outputs.c \
			   core/mllite/data_builder.c \
			   core/mllite/hal_outputs.c \
			   core/mllite/message_layer.c \
			   core/mllite/mlmath.c \
			   core/mllite/mpl.c \
			   core/mllite/results_holder.c \
			   core/mllite/start_manager.c \
			   core/mllite/storage_manager.c \
			   ../raspd/event.c \
			   ../raspd/gpiolib.c

SRCS_invmpu_dmp = src/main_dmp.c \
				  ../raspd/event.c \
				  ../raspd/gpiolib.c \

$(foreach prog, $(PROGS), $(eval OBJS_$(prog) = $(SRCS_$(prog):.c=.o)))

OBJS_libinv_mpu = $(SRCS_libinv_mpu:.c=.o)

EXT_LIBS = ../libbcm2835/libbcm2835.a \
		   ../lib/libraspberry.a \
		   ../libevent/libevent.a

.PHONY: all clean

all: $(STATIC_LIBS) $(PROGS)

libinv_mpu.a: $(OBJS_libinv_mpu)

mpu_test: $(OBJS_mpu_test) $(EXT_LIBS) libinv_mpu.a
	$(call quiet-command, $(LD) $(LDFLAGS) -o $@ $^ $(LIBS), "  LD    $(TARGET_DIR)$@")

invmpu_dmp: $(OBJS_invmpu_dmp) $(EXT_LIBS) libinv_mpu.a
	$(call quiet-command, $(LD) $(LDFLAGS) -o $@ $^ $(LIBS), "  LD    $(TARGET_DIR)$@")

ifneq ($(MAKECMDGOALS), clean)
$(foreach prog, $(PROGS), $(eval -include $(SRCS_$(prog):.c=.d)))
-include $(SRCS_libinv_mpu:.c=.d)
endif

clean:
	rm -f $(SRCS_libinv_mpu:.c=.d) $(SRCS_mpu_test:.c=.d)
	rm -f $(OBJS_libinv_mpu) $(OBJS_mpu_test)
	rm -f $(STATIC_LIBS) $(PROGS)
