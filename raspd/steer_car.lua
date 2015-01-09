-- control script

local lr = luaraspd


local l298n_stat = 0
local ultrasonic_done

ultrasonic_done = function(distance)
    io.stderr:write("auto: distance = " .. distance .. " cm\n")
    if distance <= 20 then
        lr.blink(__DEV("led_warn"), 5, 300)

        -- turn right
        lr.l298n_set(__DEV("l298n"), 3000, -2000)
        l298n_stat = 1
    elseif l298n_stat == 1 and distance > 30 then
        l298n_stat = 0
        lr.l298n_set(__DEV("l298n"), 3000, 3000)
    end
    return 0
end

function automatic()
    if lr.ultrasonic_is_busy(__DEV("ultrasonic")) then
        lr.ultrasonic_scope(__DEV("ultrasonic"), nil, 0, -1)
    end

    lr.ultrasonic_scope(__DEV("ultrasonic"), ultrasonic_done)

    lr.l298n_set(__DEV("l298n"), 3000, 3000)

    io.stderr:write("automatic leave\n")
end


local stepmotor_done
local direction = 1

stepmotor_done = function()
    direction = -direction;
    lr.stepmotor(__DEV("stepmotor"), 180 * direction, 1, stepmotor_done)
    return 0
end

--lr.stepmotor(__DEV("stepmotor"), 90, 1, stepmotor_done)
--automatic()
