# Makefile for raspberry

include rules.mak

SUBDIR_MAKEFLAGS = $(if $(V), , --no-print-directory)

TARGET_DIRS = lib catnet test

.PHONY: all

SUBDIR_RULES = $(patsubst %, subdir-%, $(TARGET_DIRS))

subdir-%:
	$(call quiet-command, $(MAKE) $(SUBDIR_MAKEFLAGS) -C $* V="$(V)" all, )

all: $(SUBDIR_RULES)
