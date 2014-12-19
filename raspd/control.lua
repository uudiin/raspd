-- ultrasonic used for obstacle avoidance

local lr = luaraspd

--lr.modexec(-1, "ultrasonic -n 999999 -t 2000")

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

-- default is output
lr.gpio_fsel(pin_laser)

lr.gpio_signal(pin_infrared_sensor, function(pin, level)
        --lev = lr.gpio_level(pin_infrared_sensor)
        lr.gpio_set(pin_laser, level)
        return 0
    end, -1)
