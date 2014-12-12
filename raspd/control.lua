-- ultrasonic used for obstacle avoidance

local lr = luaraspd

--lr.modexec(-1, "ultrasonic -n 999999 -t 2000")

function cb_ultrasonic_urgent(fd, distance)
    io.stderr:write("distance = " .. distance .. "\n")
    lr.blink(pin_led_warn, 5, 300)
    lr.modexec(fd, "l298n_lbrake; l298n_rbrake")
end

function cb_infrared_sensor()
    lr.blink(pin_laser, 5, 300)
    return 0
end

lr.gpio_signal(pin_infrared_sensor, -1, "cb_infrared_sensor")
