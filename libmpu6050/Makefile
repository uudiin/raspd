# Makefile for libbcm2835

include ../rules.mak

CFLAGS += -I ../libbcm2835 -Wall

LIBS = libmpu6050.a

SRCS_lib = i2cdev.c mpu6050.c mpu6050_6axis_motionapps20.c helper_3dmath.c

OBJS_lib = $(SRCS_lib:.c=.o)

.PHONY: all clean

all: $(LIBS)

$(LIBS): $(OBJS_lib)

ifneq ($(MAKECMDGOALS), clean)
-include $(SRCS_lib:.c=.d)
endif

clean:
	rm -f $(OBJS_lib) $(SRCS_lib:.c=.d) $(LIBS)
