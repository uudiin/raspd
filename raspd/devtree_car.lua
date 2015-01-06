-- devtree for car

devroot = {
    gpio = {
        ultrasonic = {
            {
                ID = 1,
                NAME = "ultrasonic",

                pin_trig = 20,
                pin_echo = 21,

                -- keeping pin_trig 10 us in HIGH level
                trig_time = 10
            }
        },

        stepmotor = {
            {
                ID = 2,
                NAME = "stepmotor",

                pin1 = 2,
                pin2 = 3,
                pin3 = 4,
                pin4 = 17,

                step_angle = 5.625,
                reduction_ratio = 64,
                pullin_freq = 500,
                pullout_freq = 900,
                flags = 2           -- SMF_PULSE_EIGHT
            }
        },

        l298n = {
            {
                ID = 3,
                NAME = "l298n",

                ena = 18,
                enb = 13,
                in1 = 27,
                in2 = 22,
                in3 = 5,
                in4 = 6,

                max_speed = 5,
                range = 50000,
                pwm_div = 16,
            }
        },

        simpledev = {
            { ID = 31, NAME = "led_warn",        pin = 16 },
            { ID = 32, NAME = "laster",          pin = 24 },
            { ID = 33, NAME = "infrared_sensor", pin = 23,
                cb = function(pin, level)
                    lr.gpio_set(__DEV("laster"), level)
                    return 0
                end
            },
            { ID = 34, NAME = "voice_sensor",    pin = 25,
                cb = function(pin, level)
                end
            }
        }
    },

    i2c = {},
    spi = {}
}
