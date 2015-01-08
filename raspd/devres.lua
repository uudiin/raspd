-- control script

local lr = luaraspd

resources_gpio = {}
devname = {}

local function request_gpio(config, pin)
    if resources_gpio[pin] then
        io.stderr:write("request_gpio: collisional gpio: " .. pin .. "\n")
        os.exit(1)
    end
    resources_gpio[pin] = config
end

local function register_device(dev, name)
    devname[name] = dev
end

function __DEV(name)
    return devname[name]
end

function devicetree_init(dt)
    for k, v in pairs(dt) do
        if k == "gpio" and type(v) == "table" then
            for class, devlist in pairs(v) do
                if class == "stepmotor" and type(devlist) == "table" then
                    for name, d in pairs(devlist) do
                        local stepmotor

                        request_gpio(d, d.pin1)
                        request_gpio(d, d.pin2)
                        request_gpio(d, d.pin3)
                        request_gpio(d, d.pin4)

                        -- new object
                        stepmotor = lr.stepmotor_new(d.pin1, d.pin2, d.pin3,
                                        d.pin4, d.step_angle, d.reduction_ratio,
                                        d.pullin_freq, d.pullout_freq, d.flags)
                        if stepmotor then
                            register_device(stepmotor, name)
                        else
                            io.stderr:write("stepmotor_new() error\n")
                        end
                    end
                elseif class == "ultrasonic" and type(devlist) == "table" then
                    for name, d in pairs(devlist) do
                        local ultrasonic

                        request_gpio(d, d.pin_trig)
                        request_gpio(d, d.pin_echo)
                        -- new object
                        ultrasonic = lr.ultrasonic_new(d.pin_trig,
                                                d.pin_echo, d.trig_time)
                        if ultrasonic then
                            register_device(ultrasonic, name)
                        else
                            io.stderr:write("ultrasonic_new() error\n")
                        end
                    end
                elseif class == "l298n" and type(devlist) == "table" then
                    for name, d in pairs(devlist) do
                        local l298n

                        request_gpio(d, d.ena)
                        request_gpio(d, d.enb)
                        request_gpio(d, d.in1)
                        request_gpio(d, d.in2)
                        request_gpio(d, d.in3)
                        request_gpio(d, d.in4)

                        -- new object
                        l298n = lr.l298n_new(d.ena, d.enb, d.in1, d.in2, d.in3,
                                        d.in4, d.max_speed, d.range, d.pwm_div)
                        if l298n then
                            register_device(l298n, name)
                        else
                            io.stderr:write("l298n_new() error\n")
                        end
                    end
                elseif class == "esc" and type(devlist) == "table" then
                    for name, d in pairs(devlist) do
                        local esc

                        request_gpio(d, d.pin)

                        esc = lr.esc_new(d.pin, d.refresh_rate, d.start_time,
                                    d.min_throttle_time, d.max_throttle_time)
                        if esc then
                            register_device(esc, name)
                        else
                            io.stderr:write("esc_new() error\n")
                        end
                    end
                elseif class == "tank" and type(devlist) == "table" then
                    for name, d in pairs(devlist) do
                        local tank

                        request_gpio(d, d.pin)

                        -- new object
                        tank = lr.tank_new(d.pin)
                        if tank then
                            register_device(tank, name)
                        else
                            io.stderr:write("tank_new() error\n")
                        end
                    end
                elseif class == "simpledev" and type(devlist) == "table" then
                    for name, d in pairs(devlist) do

                        request_gpio(d, d.pin)

                        if d.cb then
                            lr.gpio_signal(d.pin, d.cb)
                        else
                            lr.gpio_fsel(d.pin)
                        end

                        -- use pin as dev pointer
                        register_device(d.pin, name)
                    end
                end
            end
        elseif k == "i2c" and type(v) == "table" then
        elseif k == "spi" and type(v) == "table" then
        end
    end
end

--------------------------------------------------------------------
--------------------------------------------------------------------

if not devtree_file then
    local mach
    mach = os.getenv("RASP_MACHINE")
    if mach == "car" then
        devtree_file = config_path .. "devtree_car.lua"
    elseif mach == "tank" then
        devtree_file = config_path .. "devtree_tank.lua"
    elseif mach == "quadrotor" then
        devtree_file = config_path .. "devtree_quadcopter.lua"
    else
        devtree_file = config_path .. "devtree.lua"
    end
end

dofile(devtree_file)

-- init device tree
devicetree_init(devroot)
