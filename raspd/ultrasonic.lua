-- ultrasonic used for obstacle avoidance

local lr = luaraspd

--lr.modexec(-1, "ultrasonic -n 999999 -t 2000")

function urgent_cb(fd, distance)
    io.stderr:write("distance = " .. distance .. "\n")
    lr.blink(16, 5, 300)
    lr.modexec(fd, "l298n_lbrake; l298n_rbrake")
end
