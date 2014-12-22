-- ultrasonic used for obstacle avoidance

local lr = luaraspd

resources_gpio = {}
devid = {}

function devicetree_init(dt)
    for k, v in pairs(dt) do
        if k == "gpio" and type(v) == "table" then
            for d, c in pairs(v) do
                if d == "stepmotor" and type(c) == "table" then
                    local stepmotor

                    if resources_gpio[c.pin1] or resources_gpio[c.pin2]
                        or resources_gpio[c.pin3] or resources_gpio[c.pin4] then
                        io.stderr:write("stepmotor: collisional gpio\n")
                        os.exit(1)
                    end

                    -- register pin used
                    resources_gpio[c.pin1] = c
                    resources_gpio[c.pin2] = c
                    resources_gpio[c.pin3] = c
                    resources_gpio[c.pin4] = c

                    -- new object
                    stepmotor = lr.stepmotor_new(c.pin1, c.pin2, c.pin3, c.pin4,
                                        c.step_angle, c.reduction_ratio,
                                        c.pullin_freq, c.pullout_freq, c.flags)
                    if stepmotor then
                        devid[c.id] = stepmotor
                    else
                        io.stderr:write("stepmotor_new() error\n")
                    end
                elseif d == "ultrasonic" then
                end
            end
        elseif k == "i2c" then
        end
    end
end

function automatic_v1()
    io.stderr:write("automatic enter\n")

    if lr.ultrasonic_is_using then
        lr.ultrasonic_scope(nil, -1)
    end

    lr.ultrasonic_scope(function(distance)
            io.stderr:write("auto: distance = " .. distance .. " cm\n")
            if distance <= 15 then
                lr.blink(pin_led_warn, 5, 300)
                lr.modexec(-1, "l298n_lbrake; l298n_rbrake")
            end
            return 0
        end)

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
    io.stderr:write("stepmotor: done\n")
    direction = -direction;
    lr.stepmotor(devid[2], 180 * direction, 1, stepmotor_done)
    return 0
end

lr.stepmotor(devid[2], 90, 1, stepmotor_done)
