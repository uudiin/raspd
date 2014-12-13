-- lua config

pid_file = "/var/run/raspd.pid"
sock_file = "/var/run/raspd.sock"
chroot = ""

-- control script
script = "control.lua"

-- automatic function
automatic = "automatic"

-- pin pwm
-- channel-0  12 18
-- channel-1  13 19

-- pin of peripherals
pin_led_warn = 16
pin_infrared_sensor = 23
pin_laser = 24
pin_voice_sensor = 25

-- ultrasonic
ultrasonic = {
    ["TRIG"] = 20,
    ["ECHO"] = 21,

    -- urgent scope, 15 cm
    ["threshold"] = 15,
    ["callback"] = "cb_ultrasonic_urgent",
}

-- l298n
l298n = {
    ["ENA"] = 18,
    ["ENB"] = 13,
    ["IN1"] = 27,
    ["IN2"] = 22,
    ["IN3"] = 5,
    ["IN4"] = 6,
}
