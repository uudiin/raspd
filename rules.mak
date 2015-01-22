# Makefile rule

SHELL = bash
CROSS_COMPILE=
CC = $(CROSS_COMPILE)gcc
CXX = $(CROSS_COMPILE)g++
LD = $(CROSS_COMPILE)gcc
AR = $(CROSS_COMPILE)ar
MAKE = make
CFLAGS = -g -Wall
LDFLAGS = -g

DGFLAGS = -MMD -MP -MT $@ -MF $(*D)/$(*F).d

%.o: %.c
	$(call quiet-command, $(CC) $(CFLAGS) $(DGFLAGS) -c -o $@ $<, "  CC    $(TARGET_DIR)$@")

%.o: %.cpp
	$(call quiet-command, $(CXX) $(CFLAGS) $(DGFLAGS) -c -o $@ $<, "  CXX   $(TARGET_DIR)$@")

%.a:
	$(call quiet-command, rm -f $@ && $(AR) rcs $@ $^, "  AR    $(TARGET_DIR)$@")

%.so:
	$(call quiet-command, $(CC) $(LDFLAGS) -shared -fPIC -o $@ $^ $(LIBS), "  LD    $(TARGET_DIR)$@")

quiet-command = $(if $(V), $1, $(if $2, @echo $2 && $1, @$1))
