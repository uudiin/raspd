# Makefile for libbcm2835

include ../rules.mak

CFLAGS += -I ../libbcm2835 -I driver/include -I mllite -Wall

LIBS = libmpu.a

SRCS_lib = driver/inv_mpu.c driver/inv_mpu_dmp_motion_driver.c driver/log_stm32.c driver/platform.c \
			eMPL-hal/eMPL_outputs.c \
			mllite/data_builder.c mllite/hal_outputs.c mllite/message_layer.c \
			mllite/ml_math_func.c mllite/mlmath.c mllite/mpl.c mllite/results_holder.c \
			mllite/start_manager.c mllite/storage_manager.c

OBJS_lib = $(SRCS_lib:.c=.o)

.PHONY: all clean

all: $(LIBS)

$(LIBS): $(OBJS_lib)

ifneq ($(MAKECMDGOALS), clean)
-include $(SRCS_lib:.c=.d)
endif

clean:
	rm -f $(OBJS_lib) $(SRCS_lib:.c=.d) $(LIBS)
