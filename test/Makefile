# Makefile for test

include ../rules.mak

CFLAGS += -I ../lib -I ../libbcm2835
LIBS =


PROGS = blink_act motor breath_led pwm l298n test-raspd

SRCS_blink_act = blink_act.c
SRCS_motor = motor.c
SRCS_breath_led = breath_led.c
SRCS_pwm = pwm.c
SRCS_l298n = l298n.c
SRCS_test-raspd = test-raspd.c

$(foreach prog, $(PROGS), $(eval OBJS_$(prog) = $(SRCS_$(prog):%.c=%.o)))
$(foreach prog, $(PROGS), $(eval OBJS_$(prog) += ../libbcm2835/libbcm2835.a))

.PHONY: all clean

all: $(PROGS)

define PROG_template
$(1): $$(OBJS_$(1))
	$(call quiet-command, $$(LD) $$(LDFLAGS) -o $$@ $$^ $$(LIBS), "  LD    $$(TARGET_DIR)$$@")
clean-$(1):
	rm -f $(1) $$(SRCS_$(1):.c=.o) $$(SRCS_$(1):.c=.d)
endef

$(foreach prog, $(PROGS), $(eval $(call PROG_template,$(prog))))

ifneq ($(MAKECMDGOALS), clean)
$(foreach prog, $(PROGS), $(eval -include $(SRCS_$(prog):.c=.d)))
endif

clean: $(patsubst %, clean-%, $(PROGS))
