# Makefile for libbcm2835

include ../rules.mak

LIBS = libbcm2835.a

SRCS_lib = bcm2835.c

OBJS_lib = $(SRCS_lib:%.c=%.o)

.PHONY: all clean

all: $(LIBS)

$(LIBS): $(OBJS_lib)

clean:
	rm -f $(OBJS_lib) $(SRCS_lib:%.c=%.d) $(LIBS)
