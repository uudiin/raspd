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
        divider = 64,

        imu = {
            -- mpu6050, mpu9250
            mpu = {
                pin_int = 17,
                sample_rate = 200,
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
        pid_angle = { 10, 1, 2, 1, 6 },
        pid_rate  = { 10, 1, 2, 1, 6 },
        pid_alti  = { 10, 1, 2, 1, 6 },
    }
}
