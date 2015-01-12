# Makefile for libbcm2835

include ../rules.mak

CFLAGS += -I ../libbcm2835

LIBS = librf24.a

SRCS_lib = nrf24.c

OBJS_lib = $(SRCS_lib:.c=.o)

.PHONY: all clean

all: $(LIBS)

$(LIBS): $(OBJS_lib)

ifneq ($(MAKECMDGOALS), clean)
-include $(SRCS_lib:.c=.d)
endif

clean:
	rm -f $(OBJS_lib) $(SRCS_lib:.c=.d) $(LIBS)
