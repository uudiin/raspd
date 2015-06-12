-- devtree for quadcopter

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
    },

    softpwm = {
        -- 2.5 ms, 400 Hz
        cycle_time = 2500,  -- 2500 us
        step_time = 5,      -- 5 us

        esc = {
            min_throttle_time = 1000,   -- 1ms
            max_throttle_time = 2000,   -- 2ms

            esc1 = { pin = 27 },
            esc2 = { pin = 22 },
            esc3 = { pin = 5 },
            esc4 = { pin = 6 },
        }
    },

    i2c = {
        -- XXX: must be it
        divider = 313,

        imu = {
            -- mpu6050, mpu9250
            mpu = {
                pin_int = 4,
                -- XXX: must be it
                sample_rate = 20,
                sample_rate_final = 200,    -- FIXME
            }
        },

        barometer = {}
    },

    spi = {},

    quadcopter = {
        -- '+'
        esc_front = "esc1",
        esc_left  = "esc2",
        esc_rear  = "esc3",
        esc_right = "esc4",

        -- 'x'
        --esc_front_left  = "esc1",
        --esc_front_right = "esc2",
        --esc_rear_left   = "esc3",
        --esc_rear_right  = "esc4",

        altimeter = "ultrasonic",

        --            Kp Ki Kd min max
        pid_angle = { 0.5, 0.5, 0.55, -30, 30 },
        pid_rate  = { 0.2, 0, 0.9, -10, 10 },
        pid_alti  = { 0.5, 0, 0, -999999999, 999999999 },
    }
}
