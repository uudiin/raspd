-- lua config

pid_file = "/var/run/raspd.pid"
sock_file = "/var/run/raspd.sock"

config_path = "/root/raspberry/raspd/"

-- control script
script = "/root/raspberry/raspd/control.lua"

-- devtree
devtree_file = config_path .. "devtree_car.lua"

-- automatic function
automatic = "automatic_v1"

-- pin pwm
-- channel-0  12 18
-- channel-1  13 19


-- l298n
l298n = {
    ["ENA"] = 18,
    ["ENB"] = 13,
    ["IN1"] = 27,
    ["IN2"] = 22,
    ["IN3"] = 5,
    ["IN4"] = 6
}

-- tank
tank = {
    ["PIN"] = 7
}
