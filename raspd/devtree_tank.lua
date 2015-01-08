-- devtree for tank

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

        tank = {
            tank = { pin = 7 }
        },
    },

    i2c = {},
    spi = {}
}
