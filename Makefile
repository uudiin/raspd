# Makefile for raspberry

include rules.mak

SUBDIR_MAKEFLAGS = $(if $(V), , --no-print-directory)

TARGET_DIRS = libbcm2835 lib libevent librf24 catnet client raspd test

.PHONY: all clean distclean

SUBDIR_RULES = $(patsubst %, subdir-%, $(TARGET_DIRS))

subdir-%:
	$(call quiet-command, $(MAKE) $(SUBDIR_MAKEFLAGS) -C $* V="$(V)" TARGET_DIR="$*/" all, )

all: $(SUBDIR_RULES)


# clean
CLEAN_SUBDIRS = $(patsubst %, clean_subdir-%, $(TARGET_DIRS))

clean_subdir-%:
	$(call quiet-command, $(MAKE) $(SUBDIR_MAKEFLAGS) -C $* V="$(V)" clean, )

clean: $(CLEAN_SUBDIRS)

# distclean
# FIXME equal clean
DC_SUBDIRS = $(patsubst %, dc_subdir-%, $(TARGET_DIRS))
$(foreach D, $(TARGET_DIRS), $(eval dc_subdir-$(D): ; $(call quiet-command, $(MAKE) $(SUBDIR_MAKEFLAGS) -C $(D) V="$(V)" clean, )))
distclean: $(DC_SUBDIRS)
