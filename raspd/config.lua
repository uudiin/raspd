-- lua config

pid_file = "/var/run/raspd.pid"
sock_file = "/var/run/raspd.sock"
chroot = ""

-- ultrasonic
ultrasonic = {
    ["TRIG"] = 20,
    ["ECHO"] = 21,

    -- urgent scope, 10 cm
    ["threshold"] = 10,
    ["script"] = "ultrasonic.lua",
    ["callback"] = "urgent_cb",
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
