# Makefile rule

SHELL = bash
CC = gcc
LD = gcc
MAKE = make
CFLAGS = -g

DGFLAGS = -MMD -MP -MT $@ -MF $(*D)/$(*F).d

%.o: %.c
	$(call quiet-command, $(CC) $(CFLAGS) $(DGFLAGS) -c -o $@ $<, "  CC    $@")

%.a:
	$(call quiet-command, rm -f $@ && $(AR) rcs $@ $^, "  AR    $@")

quiet-command = $(if $(V), $1, $(if $2, @echo $2 && $1, @$1))
