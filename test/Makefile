# Makefile for test

SHELL = bash
CC = gcc
LD = gcc
CFLAGS = -g
LDFLAGS = -g
LIBS = -lbcm2835

DGFLAGS = -MMD -MP -MT $@ -MF $(*D)/$(*F).d

PROGS = blink_act motor breath_led

SRCS_blinkact = blink_act.c
SRCS_motor = motor.c
SRCS_breath_led = breath_led.c

$(foreach prog, $(PROGS), $(eval OBJS_$(prog) = $(SRCS_$(prog):%.c=%.o)))

.PHONY: all clean

all: $(PROGS)

blink_act: $(OBJS_blink_act)
	$(call quiet-command, $(LD) $(LDFLAGS) -o $@ $^ $(LIBS), "  LD    $@")

motor: $(OBJS_motor)
	$(call quiet-command, $(LD) $(LDFLAGS) -o $@ $^ $(LIBS), "  LD    $@")

breath_led: $(OBJS_breath_led)
	$(call quiet-command, $(LD) $(LDFLAGS) -o $@ $^ $(LIBS), "  LD    $@")

%.o: %.c
	$(call quiet-command, $(CC) $(CFLAGS) $(DGFLAGS) -c -o $@ $<, "  CC    $@")

ifneq ($(MAKECMDGOALS), clean)
$(foreach prog, $(PROGS), $(eval -include $(SRCS_$(prog):.c=.d)))
endif

clean:
	rm -f *.d $(OBJS_blink_act) $(OBJS_motor) $(OBJS_breath_led) $(PROGS)

quiet-command = $(if $(V), $1, $(if $2, @echo $2 && $1, @$1))
