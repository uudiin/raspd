-- ultrasonic used for obstacle avoidance

local lr = luaraspd

--lr.blink(gpio, n, t)
--lr.pwm(gpio, range, ...)
--lr.l298n(...)

function urgent_cb(fd, distance)
    --io.stderr:write("distance = " .. distance)
    io.stderr:write("ultrasonic.lua  distance = " .. "lua")
end
