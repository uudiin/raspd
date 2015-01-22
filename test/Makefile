# Makefile for test

include ../rules.mak

CFLAGS += -Wno-pointer-to-int-cast -Wno-int-to-pointer-cast -Wall
CFLAGS += -I ../lib -I ../libbcm2835 -I../libevent/include -I ../librf24
LIBS =


PROGS = blink_act motor breath_led pwm l298n test_unix softpwm_test \
		rt_ssh test_gpioint sw2 rf24_test esc_test mpu6050-test

$(foreach prog, $(PROGS), $(eval SRCS_$(prog) = $(prog).c))

SRCS_test_gpioint += ../raspd/event.c ../raspd/gpiolib.c
SRCS_rf24_test += ../raspd/event.c ../raspd/gpiolib.c
SRCS_hcsr04 += ../raspd/event.c ../raspd/gpiolib.c
SRCS_sw += ../raspd/event.c
SRCS_rf24_test += ../raspd/event.c ../raspd/gpiolib.c
SRCS_softpwm_test += ../raspd/softpwm.c


$(foreach prog, $(PROGS), $(eval OBJS_$(prog) = $(SRCS_$(prog):.c=.o)))
$(foreach prog, $(PROGS), $(eval OBJS_$(prog) += \
	../libbcm2835/libbcm2835.a ../lib/libraspberry.a ../libevent/libevent.a ../librf24/librf24.a))

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
