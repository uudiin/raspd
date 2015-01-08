-- devtree for quadrotor

devroot = {
    gpio = {
        ultrasonic = {
            ultrasonic = {
                pin_trig = 20,
                pin_echo = 21,

                -- keeping pin_trig 10 us in HIGH level
                trig_time = 10
            }
        },

        stepmotor = {
            stepmotor = {
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

        esc = {
            esc1 = {
                pin = 27,

                refresh_rate = 50,      -- frequency (hz)
                start_time = 5000,      -- 5s
                min_throttle_time = 900,    -- 900us
                max_throttle_time = 2100,   -- 2100us
            },
            esc2 = {
                pin = 22,

                refresh_rate = 50,      -- frequency (hz)
                start_time = 5000,      -- 5s
                min_throttle_time = 900,    -- 900us
                max_throttle_time = 2100,   -- 2100us
            },
            esc3 = {
                pin = 5,

                refresh_rate = 50,      -- frequency (hz)
                start_time = 5000,      -- 5s
                min_throttle_time = 900,    -- 900us
                max_throttle_time = 2100,   -- 2100us
            },
            esc4 = {
                pin = 6,

                refresh_rate = 50,      -- frequency (hz)
                start_time = 5000,      -- 5s
                min_throttle_time = 900,    -- 900us
                max_throttle_time = 2100,   -- 2100us
            }
        }
    },

    i2c = {},
    spi = {}
}