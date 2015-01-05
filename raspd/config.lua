-- lua config

pid_file = "/var/run/raspd.pid"
sock_file = "/var/run/raspd.sock"

config_path = "/root/raspberry/raspd/"

-- control script
script = "/root/raspberry/raspd/control.lua"

-- devtree
--devtree_file = "/root/raspberry/raspd/devtree_quadrotor.lua"
devtree_file = config_path .. "devtree_quadrotor.lua"

if not devtree_file then
    local mach
    mach = os.getenv("RASP_MACHINE")
    if mach == "car" then
        --devtree_file = "/root/raspberry/raspd/devtree_car.lua"
        devtree_file = config_path .. "devtree_car.lua"
    elseif mach == "tank" then
        --devtree_file = "/root/raspberry/raspd/devtree_tank.lua"
        devtree_file = config_path .. "devtree_tank.lua"
    elseif mach == "quadrotor" then
        --devtree_file = "/root/raspberry/raspd/devtree_quadrotor.lua"
        devtree_file = config_path .. "devtree_quadrotor.lua"
    else
        --devtree_file = "/root/raspberry/raspd/devtree.lua"
        devtree_file = config_path .. "devtree.lua"
    end
end

if not dofile(devtree_file) then
    io.stderr:write("load devtree error: " .. devtree_file .. "\n")
    os.exit(1)
end

-- automatic function
automatic = "automatic_v1"

-- pin pwm
-- channel-0  12 18
-- channel-1  13 19

-- pin of peripherals
pin_led_warn = 16
pin_infrared_sensor = 23
pin_laser = 24
pin_voice_sensor = 25

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
