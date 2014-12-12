-- lua config

pid_file = "/var/run/raspd.pid"
sock_file = "/var/run/raspd.sock"
chroot = ""

script = "control.lua"

pin_led_warn = 16
pin_infrared_sensor = XX
pin_laser = XX
pin_voice_sensor = XX

-- ultrasonic
ultrasonic = {
    ["TRIG"] = 20,
    ["ECHO"] = 21,

    -- urgent scope, 10 cm
    ["threshold"] = 10,
    ["callback"] = "cb_ultrasonic_urgent",
}

-- l298n
l298n = {
    ["ENA"] = 18,
    ["ENB"] = 13,
    ["IN1"] = 17,
    ["IN2"] = 27,
    ["IN3"] = 22,
    ["IN4"] = 23,
}
