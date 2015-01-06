-- lua config

pid_file = "/var/run/raspd.pid"
sock_file = "/var/run/raspd.sock"

config_path = "/root/raspberry/raspd/"

-- control script
script = "/root/raspberry/raspd/control.lua"

-- devtree
--devtree_file = "/root/raspberry/raspd/devtree_quadrotor.lua"
devtree_file = config_path .. "devtree_tank.lua"

-- automatic function
automatic = "automatic_v1"

-- pin pwm
-- channel-0  12 18
-- channel-1  13 19
