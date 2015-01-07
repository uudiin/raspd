-- control script

local lr = luaraspd


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
