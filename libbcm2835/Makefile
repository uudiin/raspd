# Makefile for libbcm2835

include ../rules.mak

CFLAGS += -Wno-pointer-to-int-cast

LIBS = libbcm2835.a

SRCS_lib = bcm2835.c

OBJS_lib = $(SRCS_lib:.c=.o)

.PHONY: all clean

all: $(LIBS)

$(LIBS): $(OBJS_lib)

ifneq ($(MAKECMDGOALS), clean)
-include $(SRCS_lib:.c=.d)
endif

clean:
	rm -f $(OBJS_lib) $(SRCS_lib:.c=.d) $(LIBS)
