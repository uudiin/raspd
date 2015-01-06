-- control script

local lr = luaraspd

resources_gpio = {}
devid = {}
devname = {}

local function request_gpio(config, pin)
    if resources_gpio[pin] then
        io.stderr:write("request_gpio: collisional gpio: " .. pin .. "\n")
        os.exit(1)
    end
    resources_gpio[pin] = config
end

local function register_device(dev, id, name)
    if id then
        devid[id] = dev
    end

    if name then
        devname[name] = dev
    end
end

function __DEV(id)
    if devid[id] then
        return devid[id]
    else
        return devname[id]
    end
end

function devicetree_init(dt)
    for k, v in pairs(dt) do
        if k == "gpio" and type(v) == "table" then
            for class, devlist in pairs(v) do
                if class == "stepmotor" and type(devlist) == "table" then
                    for index, d in ipairs(devlist) do
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
                            register_device(stepmotor, d.ID, d.NAME)
                        else
                            io.stderr:write("stepmotor_new() error\n")
                        end
                    end
                elseif class == "ultrasonic" and type(devlist) == "table" then
                    for index, d in ipairs(devlist) do
                        local ultrasonic

                        request_gpio(d, d.pin_trig)
                        request_gpio(d, d.pin_echo)
                        -- new object
                        ultrasonic = lr.ultrasonic_new(d.pin_trig,
                                                    d.pin_echo, d.trig_time)
                        if ultrasonic then
                            register_device(ultrasonic, d.ID, d.NAME)
                        else
                            io.stderr:write("ultrasonic_new() error\n")
                        end
                    end
                elseif class == "l298n" and type(devlist) == "table" then
                    for index, d in ipairs(devlist) do
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
                            register_device(l298n, d.ID, d.NAME)
                        else
                            io.stderr:write("l298n_new() error\n")
                        end
                    end
                elseif class == "esc" and type(devlist) == "table" then
                    for index, d in ipairs(devlist) do
                        local esc

                        request_gpio(d, d.pin)

                        esc = lr.esc_new(d.pin, d.refresh_rate, d.start_time,
                                    d.min_throttle_time, d.max_throttle_time)
                        if esc then
                            register_device(esc, d.ID, d.NAME)
                        else
                            io.stderr:write("esc_new() error\n")
                        end
                    end
                elseif class == "simpledev" and type(devlist) == "table" then
                    for index, d in ipairs(devlist) do
                        if d.cb then
                            lr.gpio_signal(d.pin, d.cb)
                        else
                            lr.gpio_fsel(d.pin)
                        end
                        request_gpio(d, d.pin)
                        -- use pin as dev pointer
                        register_device(d.pin, d.ID, d.NAME)
                    end
                end
            end
        elseif k == "i2c" and type(v) == "table" then
        elseif k == "spi" and type(v) == "table" then
        end
    end
end

local l298n_stat = 0
local ultrasonic_done

ultrasonic_done = function(distance)
    io.stderr:write("auto: distance = " .. distance .. " cm\n")
    if distance <= 30 then
        lr.blink(__DEV("led_warn"), 5, 300)
        --lr.modexec(-1, "l298n_lbrake; l298n_rbrake")

        -- turn right
        --lr.modexec(-1, "l298n_rdown; tank_right")
        l298n_stat = 1
    elseif l298n_stat == 1 and distance > 50 then
        l298n_stat = 0
        --lr.modexec(-1, "l298n_rup; tank_brake; tank_fwd")
    end
    -- start again
    --lr.ultrasonic(__DEV("ultrasonic"), ultrasonic_done)
    return 0
end

function automatic_v1()
    io.stderr:write("automatic enter\n")

    --if lr.ultrasonic_is_using() then
    --    lr.ultrasonic_scope0(nil, 0, -1)
    --end

    lr.ultrasonic_scope(__DEV("ultrasonic"), ultrasonic_done)

    --lr.modexec(-1, "l298n_lup; l298n_rup; tank_sup; tank_fwd")
    --lr.modexec(-1, "l298n_lspeedup; l298n_rspeedup")
    --lr.modexec(-1, "l298n_lspeedup; l298n_rspeedup")
    --lr.modexec(-1, "l298n_lspeedup; l298n_rspeedup")

    io.stderr:write("automatic leave\n")
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
        devtree_file = config_path .. "devtree_quadrotor.lua"
    else
        devtree_file = config_path .. "devtree.lua"
    end
end

dofile(devtree_file)

--if not dofile(devtree_file) then
--    io.stderr:write("load devtree error: " .. devtree_file .. "\n")
--    os.exit(1)
--end

-- init device tree
devicetree_init(devroot)


local stepmotor_done
local direction = 1

stepmotor_done = function()
    --io.stderr:write("stepmotor: done\n")
    direction = -direction;
    lr.stepmotor(__DEV("stepmotor"), 180 * direction, 1, stepmotor_done)
    return 0
end

--lr.stepmotor(__DEV("stepmotor"), 90, 1, stepmotor_done)
--lr.modexec(-1, "automatic")
