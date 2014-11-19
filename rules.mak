# Makefile rule

SHELL = bash
CC = gcc
CXX = g++
LD = gcc
MAKE = make
CFLAGS = -g
LDFLAGS = -g

DGFLAGS = -MMD -MP -MT $@ -MF $(*D)/$(*F).d

%.o: %.c
	$(call quiet-command, $(CC) $(CFLAGS) $(DGFLAGS) -c -o $@ $<, "  CC    $(TARGET_DIR)$@")

%.o: %.cpp
	$(call quiet-command, $(CXX) $(CFLAGS) $(DGFLAGS) -c -o $@ $<, "  CXX    $(TARGET_DIR)$@")

%.a:
	$(call quiet-command, rm -f $@ && $(AR) rcs $@ $^, "  AR    $(TARGET_DIR)$@")

quiet-command = $(if $(V), $1, $(if $2, @echo $2 && $1, @$1))
