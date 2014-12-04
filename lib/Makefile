# Makefile for lib

include ../rules.mak

CFLAGS += -fPIC

LIBS = libraspberry.a

SRCS_lib = sock.c unix.c utils.c xmalloc.c

OBJS_lib = $(SRCS_lib:.c=.o)

.PHONY: all clean

all: $(LIBS)

$(LIBS): $(OBJS_lib)

ifneq ($(MAKECMDGOALS), clean)
-include $(SRCS_lib:.c=.d)
endif

clean:
	rm -f $(OBJS_lib) $(SRCS_lib:.c=.d) $(LIBS)
