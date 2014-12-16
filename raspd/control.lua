-- ultrasonic used for obstacle avoidance

local lr = luaraspd

--lr.modexec(-1, "ultrasonic -n 999999 -t 2000")

function automatic_v1()
    io.stderr:write("automatic enter\n")
end

function cb_ultrasonic_urgent(fd, distance)
    io.stderr:write("distance = " .. distance .. " cm\n")
    lr.blink(pin_led_warn, 5, 300)
    lr.modexec(fd, "l298n_lbrake; l298n_rbrake")
end

-- default is output
lr.gpio_fsel(pin_laser)

lr.gpio_signal(pin_infrared_sensor, function()
    lev = lr.gpio_level(pin_infrared_sensor)
    lr.gpio_set(pin_laser, lev)
    return 0
end, -1)
