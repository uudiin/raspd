# Makefile for mpu motion driver

include ../rules.mak

INC_PATHS = core/driver/include \
			core/driver/eMPL \
			core/eMPL-hal \
			core/mllite \
			core/mpl \
			core/driver \
			../lib \
			../libbcm2835 \
			../libevent/include

INCLUDES = $(addprefix -I, $(INC_PATHS))

MACRO_DEFS = EMPL_TARGET_BCM2835 \
			 EMPL \
			 LINUX \
			 USE_CAL_HW_REGISTERS \
			 LOG_STD

ifeq ($(ARCH),arm)
MACRO_DEFS += MPL_STATIC_LIB
endif

MACROS = $(addprefix -D, $(MACRO_DEFS))

CFLAGS += $(INCLUDES) $(MACROS)
LDFLAGS += -lm
LIBS =

STATIC_LIBS = libinv_mpu.a libinv_mpu_9250.a
PROGS = invmpu_dmp mpu_test

SRCS_libinv_mpu = core/driver/eMPL/inv_mpu.c \
                  core/driver/eMPL/inv_mpu_dmp_motion_driver.c \
                  core/eMPL-hal/eMPL_outputs.c \
                  core/mllite/ml_math_func.c \
                  core/mllite/data_builder.c \
                  core/mllite/hal_outputs.c \
                  core/mllite/message_layer.c \
                  core/mllite/mlmath.c \
                  core/mllite/mpl.c \
                  core/mllite/results_holder.c \
                  core/mllite/start_manager.c \
                  core/mllite/storage_manager.c

SRCS_mpu_test = src/main.c \
			   core/driver/log_std.c \
			   ../raspd/event.c \
			   ../raspd/gpiolib.c

SRCS_invmpu_dmp = src/main_dmp.c \
			   src/log_network.c \
			   ../raspd/event.c \
			   ../raspd/gpiolib.c

$(foreach prog, $(PROGS), $(eval OBJS_$(prog) = $(SRCS_$(prog):.c=.o)))

OBJS_libinv_mpu = $(SRCS_libinv_mpu:.c=.o)
OBJS_libinv_mpu_9250 = $(SRCS_libinv_mpu:.c=.9250o)


MPL_LIBS = core/mpl/quaternion_supervisor.o \
				core/mpl/no_gyro_fusion.o \
				core/mpl/motion_no_motion.o \
				core/mpl/mag_disturb.o \
				core/mpl/heading_from_gyro.o \
				core/mpl/gyro_tc.o \
				core/mpl/fusion_9axis.o \
				core/mpl/fast_no_motion.o \
				core/mpl/compass_vec_cal.o \
				core/mpl/accel_auto_cal.o

ifeq ($(ARCH),arm)
OBJS_libinv_mpu += $(MPL_LIBS)
OBJS_libinv_mpu_9250 += $(MPL_LIBS)
endif

EXT_LIBS = ../libbcm2835/libbcm2835.a \
		   ../lib/libraspberry.a \
		   ../libevent/libevent.a

#OBJS_invmpu_dmp_9250 = $(addprefix objs9250/, $(OBJS_invmpu_dmp) $(OBJS_libinv_mpu))
#
#objs9250/%.o: %.c
#	$(call quiet-command, $(CC) $(CFLAGS) $(DGFLAGS) -c -o $@ $<, "  CC    $(TARGET_DIR)$@")

OBJS_mpu_test_9250 = $(OBJS_mpu_test:.o=.9250o)
OBJS_invmpu_dmp_9250 = $(OBJS_invmpu_dmp:.o=.9250o)

%.9250o: %.c
	$(call quiet-command, $(CC) $(CFLAGS) $(DGFLAGS) -c -o $@ $<, "  CC    $(TARGET_DIR)$@")


.PHONY: all clean

all: $(STATIC_LIBS) $(PROGS) mpu_test_9250 invmpu_dmp_9250

mpu_test libinv_mpu.a: CFLAGS += -DMPU6050

mpu_test_9250 libinv_mpu_9250.a: CFLAGS += -DMPU9250 -DCOMPASS_ENABLED

invmpu_dmp libinv_mpu.a: CFLAGS += -DMPU6050

invmpu_dmp_9250 libinv_mpu_9250.a: CFLAGS += -DMPU9250 -DCOMPASS_ENABLED


libinv_mpu.a: $(OBJS_libinv_mpu)

libinv_mpu_9250.a: $(OBJS_libinv_mpu_9250)

mpu_test: $(OBJS_mpu_test) $(OBJS_libinv_mpu) $(EXT_LIBS)
	$(call quiet-command, $(LD) $(LDFLAGS) -o $@ $^ $(LIBS), "  LD    $(TARGET_DIR)$@")

mpu_test_9250: $(OBJS_mpu_test_9250) $(OBJS_libinv_mpu_9250) $(EXT_LIBS)
	$(call quiet-command, $(LD) $(LDFLAGS) -o $@ $^ $(LIBS), "  LD    $(TARGET_DIR)$@")

invmpu_dmp: $(OBJS_invmpu_dmp) $(EXT_LIBS) libinv_mpu.a
	$(call quiet-command, $(LD) $(LDFLAGS) -o $@ $^ $(LIBS), "  LD    $(TARGET_DIR)$@")

invmpu_dmp_9250: $(OBJS_invmpu_dmp_9250) $(EXT_LIBS) libinv_mpu_9250.a
	$(call quiet-command, $(LD) $(LDFLAGS) -o $@ $^ $(LIBS), "  LD    $(TARGET_DIR)$@")


ifneq ($(MAKECMDGOALS), clean)
$(foreach prog, $(PROGS), $(eval -include $(SRCS_$(prog):.c=.d)))
-include $(SRCS_libinv_mpu:.c=.d)
endif

clean:
	rm -f $(SRCS_libinv_mpu:.c=.d) $(SRCS_mpu_test:.c=.d) $(SRCS_invmpu_dmp:.c=.d)
	rm -f $(OBJS_libinv_mpu) $(OBJS_libinv_mpu_9250) $(OBJS_mpu_test) $(OBJS_invmpu_dmp) $(OBJS_invmpu_dmp_9250)
	rm -f $(STATIC_LIBS) $(PROGS) invmpu_dmp_9250
