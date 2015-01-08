-- lua config

pid_file = "/var/run/raspd.pid"
sock_file = "/var/run/raspd.sock"

config_path = "/root/raspberry/raspd/"

-- device resources
devres = config_path .. "devres.lua"

-- steer
steer = config_path .. "steer_car.lua"

-- devtree
--devtree_file = "/root/raspberry/raspd/devtree_quadrotor.lua"
devtree_file = config_path .. "devtree_tank.lua"

-- pin pwm
-- channel-0  12 18
-- channel-1  13 19
