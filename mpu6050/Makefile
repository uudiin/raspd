# Makefile for mpu motion driver

include ../rules.mak

INC_PATHS = ../libbcm2835 \
			core/driver/include \
			core/driver/eMPL \
			core/eMPL-hal \
			core/mllite \
			core/mpl \
			core/driver/stm32L

INCLUDES = $(addprefix -I, $(INC_PATHS))

CFLAGS += $(INCLUDES)
CFLAGS += -DEMPL_TARGET_BCM2835 -DMPU6050 -DLINUX
LDFLAGS += -lm
LIBS =

STATIC_LIBS = libmpu6050.a
PROGS = mpu6050

SRCS_libmpu6050 = core/driver/eMPL/inv_mpu.c \
				  core/driver/eMPL/inv_mpu_dmp_motion_driver.c \
				  core/driver/stm32L/log_stm32l.c \
				  core/eMPL-hal/eMPL_outputs.c \
				  core/mllite/data_builder.c \
				  core/mllite/hal_outputs.c \
				  core/mllite/message_layer.c \
				  core/mllite/ml_math_func.c \
				  core/mllite/mlmath.c \
				  core/mllite/mpl.c \
				  core/mllite/results_holder.c \
				  core/mllite/start_manager.c \
				  core/mllite/storage_manager.c

SRCS_mpu6050 = src/main.c

$(foreach prog, $(PROGS), $(eval OBJS_$(prog) = $(SRCS_$(prog):.c=.o)))

OBJS_libmpu6050 = $(SRCS_libmpu6050:.c=.o)

.PHONY: all clean

all: $(STATIC_LIBS) $(PROGS)

libmpu6050.a: $(OBJS_libmpu6050)

mpu6050: $(OBJS_mpu6050) libmpu6050.a ../libbcm2835/libbcm2835.a
	$(call quiet-command, $(LD) $(LDFLAGS) -o $@ $^ $(LIBS), "  LD    $(TARGET_DIR)$@")

ifneq ($(MAKECMDGOALS), clean)
$(foreach prog, $(PROGS), $(eval -include $(SRCS_$(prog):.c=.d)))
-include $(SRCS_libmpu6050:.c=.d)
endif

clean:
	rm -f $(SRCS_libmpu6050:.c=.d) $(SRCS_mpu6050:.c=.d)
	rm -f $(OBJS_libmpu6050) $(OBJS_mpu6050)
	rm -f $(STATIC_LIBS) $(PROGS)
