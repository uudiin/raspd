# Makefile for libbcm2835

include ../rules.mak

CFLAGS += -I ../libbcm2835 -Wall

LIBS = libmpu6050.a

SRCS_lib = mpu6050.c

OBJS_lib = $(SRCS_lib:.c=.o)

.PHONY: all clean

all: $(LIBS)

$(LIBS): $(OBJS_lib)

ifneq ($(MAKECMDGOALS), clean)
-include $(SRCS_lib:.c=.d)
endif

clean:
	rm -f $(OBJS_lib) $(SRCS_lib:.c=.d) $(LIBS)
