-- devtree for tank

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
    },

    i2c = {},
    spi = {}
}
