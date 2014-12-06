# Makefile for client

include ../../rules.mak
CFLAGS += -g -fPIC
LDFLAGS += -llog
LIBS =
CROSS_COMPILE=arm-linux-androideabi-

SRCS_client = client-jni.c
OBJS_client = client-jni.o

.PHONY: all clean

libclient.so: $(OBJS_client) ../libclient.a ../../libevent/libevent.a ../../lib/libraspberry.a

clean:
	rm -f *.d $(OBJS_client) libclient.so
