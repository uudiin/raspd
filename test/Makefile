# Makefile for test

include ../rules.mak

CFLAGS += -I ../lib -I ../libbcm2835
LIBS =


PROGS = blink_act motor breath_led pwm

SRCS_blink_act = blink_act.c
SRCS_motor = motor.c
SRCS_breath_led = breath_led.c
SRCS_pwm = pwm.c

$(foreach prog, $(PROGS), $(eval OBJS_$(prog) = $(SRCS_$(prog):%.c=%.o)))
$(foreach prog, $(PROGS), $(eval OBJS_$(prog) += ../libbcm2835/libbcm2835.a))

.PHONY: all clean

all: $(PROGS)

blink_act: $(OBJS_blink_act)
	$(call quiet-command, $(LD) $(LDFLAGS) -o $@ $^ $(LIBS), "  LD    $(TARGET_DIR)$@")

motor: $(OBJS_motor)
	$(call quiet-command, $(LD) $(LDFLAGS) -o $@ $^ $(LIBS), "  LD    $(TARGET_DIR)$@")

breath_led: $(OBJS_breath_led)
	$(call quiet-command, $(LD) $(LDFLAGS) -o $@ $^ $(LIBS), "  LD    $(TARGET_DIR)$@")

pwm: $(OBJS_pwm)
	$(call quiet-command, $(LD) $(LDFLAGS) -o $@ $^ $(LIBS), "  LD    $(TARGET_DIR)$@")

#define PROG_template =
#$(1): $$(OBJS_$(1)); $(call ...)
#end
#
#$(foreach prog, $(PROGS), $(eval $(prog): $(OBJS_$(prog)); $(call quiet-command, $(LD) $(LDFLAGS) -o $@ $^ $(LIBS), "  LD    $(TARGET_DIR)$@")))

ifneq ($(MAKECMDGOALS), clean)
$(foreach prog, $(PROGS), $(eval -include $(SRCS_$(prog):.c=.d)))
endif

clean:
	rm -f *.d $(OBJS_blink_act) $(OBJS_motor) $(OBJS_breath_led) $(OBJS_pwm) $(PROGS)
