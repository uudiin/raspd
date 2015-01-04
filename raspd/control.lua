-- ultrasonic used for obstacle avoidance

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
            for d, c in pairs(v) do
                if d == "stepmotor" and type(c) == "table" then
                    local stepmotor

                    request_gpio(c, c.pin1)
                    request_gpio(c, c.pin2)
                    request_gpio(c, c.pin3)
                    request_gpio(c, c.pin4)

                    -- new object
                    stepmotor = lr.stepmotor_new(c.pin1, c.pin2, c.pin3, c.pin4,
                                        c.step_angle, c.reduction_ratio,
                                        c.pullin_freq, c.pullout_freq, c.flags)
                    if stepmotor then
                        register_device(stepmotor, c.ID, c.NAME)
                    else
                        io.stderr:write("stepmotor_new() error\n")
                    end
                elseif d == "ultrasonic" and type(c) == "table" then
                    local ultrasonic

                    request_gpio(c, c.pin_trig)
                    request_gpio(c, c.pin_echo)
                    -- new object
                    ultrasonic = lr.ultrasonic_new(c.pin_trig,
                                                c.pin_echo, c.trig_time)
                    if ultrasonic then
                        register_device(ultrasonic, c.ID, c.NAME)
                    else
                        io.stderr:write("ultrasonic_new() error\n")
                    end
                end
            end
        elseif k == "i2c" then
        end
    end
end

local l298n_stat = 0
local ultrasonic_done

ultrasonic_done = function(distance)
    io.stderr:write("auto: distance = " .. distance .. " cm\n")
    if distance <= 30 then
        lr.blink(pin_led_warn, 5, 300)
        --lr.modexec(-1, "l298n_lbrake; l298n_rbrake")

        -- turn right
        lr.modexec(-1, "l298n_rdown; tank_right")
        l298n_stat = 1
    elseif l298n_stat == 1 and distance > 50 then
        l298n_stat = 0
        lr.modexec(-1, "l298n_rup; tank_brake; tank_fwd")
    end
    -- start again
    lr.ultrasonic(__DEV("ultrasonic"), ultrasonic_done)
    return 0
end

function automatic_v1()
    io.stderr:write("automatic enter\n")

    --if lr.ultrasonic_is_using() then
    --    lr.ultrasonic_scope(nil, 0, -1)
    --end

    lr.ultrasonic(__DEV("ultrasonic"), ultrasonic_done)

    lr.modexec(-1, "l298n_lup; l298n_rup; tank_sup; tank_fwd")
    lr.modexec(-1, "l298n_lspeedup; l298n_rspeedup")
    lr.modexec(-1, "l298n_lspeedup; l298n_rspeedup")
    lr.modexec(-1, "l298n_lspeedup; l298n_rspeedup")

    io.stderr:write("automatic leave\n")
end

function cb_ultrasonic_urgent(fd, distance)
    io.stderr:write("distance = " .. distance .. " cm\n")
    lr.blink(pin_led_warn, 5, 300)
    lr.modexec(fd, "l298n_lbrake; l298n_rbrake")
end


-- init device tree
devicetree_init(devroot)

-- default is output
lr.gpio_fsel(pin_laser)

lr.gpio_signal(pin_infrared_sensor, function(pin, level)
        --lev = lr.gpio_level(pin_infrared_sensor)
        lr.gpio_set(pin_laser, level)
        return 0
    end, -1)


local stepmotor_done
local direction = 1

stepmotor_done = function()
    --io.stderr:write("stepmotor: done\n")
    direction = -direction;
    lr.stepmotor(__DEV("stepmotor"), 180 * direction, 1, stepmotor_done)
    return 0
end

--lr.stepmotor(__DEV("stepmotor"), 90, 1, stepmotor_done)
lr.modexec(-1, "automatic")
