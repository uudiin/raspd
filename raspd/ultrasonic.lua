-- ultrasonic used for obstacle avoidance

local lr = luaraspd

function urgent_cb(fd, distance)
    io.stderr:write("distance = " .. distance .. "\n")
    lr.blink(18, 5, 300)
    lr.modexec("l298n_lbrake; l298n_rbrake")
end
