# Makefile for libbcm2835

include ../rules.mak

CFLAGS += -I ../libbcm2835

LIBS = librf24.a

SRCS_lib = RF24.cpp

OBJS_lib = $(SRCS_lib:.cpp=.o)

.PHONY: all clean

all: $(LIBS)

$(LIBS): $(OBJS_lib)

ifneq ($(MAKECMDGOALS), clean)
-include $(SRCS_lib:.cpp=.d)
endif

clean:
	rm -f $(OBJS_lib) $(SRCS_lib:.cpp=.d) $(LIBS)
