#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <errno.h>
#include <event2/event.h>

#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>

#include <bcm2835.h>

#include <xmalloc.h>

#include "module.h"
#include "event.h"
#include "gpiolib.h"
#include "gpio.h"
#include "pwm.h"
#include "ultrasonic.h"
#include "motor.h"
#include "l298n.h"
#include "tankcontrol.h"
#include "softpwm.h"
#include "inv_imu.h"
#include "pidctrl.h"

#include "luaenv.h"

static lua_State *_L;

#ifdef _DEBUG_LUAENV

static void stack_dump(lua_State *L, FILE *fp)
{
    int i;
    int top = lua_gettop(L);

    for (i = 1; i <= top; i++) {
        int t = lua_type(L, i);
        switch (t) {
        case LUA_TSTRING: fprintf(fp, "'%s'", lua_tostring(L, i)); break;
        case LUA_TBOOLEAN: fprintf(fp, lua_toboolean(L, i) ? "true" : "false"); break;
        case LUA_TNUMBER: fprintf(fp, "%g", lua_tonumber(L, i)); break;
        default: fprintf(fp, "%s", lua_typename(L, t)); break;
        }
        fprintf(fp, " ");
    }
    fprintf(fp, "\n");
}

#endif

struct timeout_env {
    int timeout;    /* ms */
    int count;
    int counted;
    struct event *ev;
};

static void cb_timeout_wrap(int fd, short what, void *arg)
{
    struct timeout_env *env = arg;

    if (env->counted != -1 && env->counted++ >= env->count) {
        eventfd_del(env->ev);
        free(env);
        return;
    }

    /* get lua handler */
    lua_pushlightuserdata(_L, &_L);
    lua_rawget(_L, LUA_REGISTRYINDEX);
    lua_pushinteger(_L, (intptr_t)env);
    lua_gettable(_L, -2);

    /* call lua handler */
    if (lua_pcall(_L, 0, 1, 0) == 0) {
        int retval = luaL_checkinteger(_L, -1);
        if (retval < 0) {
            eventfd_del(env->ev);
            free(env);
        }
        lua_pop(_L, 1);
    }
}

/* timeout, cb, count */
static int lr_timeout(lua_State *L)
{
    struct timeout_env *env;
    struct timeval tv;
    int timeout = (int)luaL_checkinteger(L, 1);
    int count = (int)luaL_optint(L, 3, 1); /* default once */
    int err = -ENOMEM;

    if (timeout <= 0)
        return 0;
    if (!lua_isfunction(L, 2) || lua_iscfunction(L, 2))
        return 0;
    env = malloc(sizeof(*env));
    if (env) {
        memset(env, 0, sizeof(*env));
        env->timeout = timeout;
        env->count = count;

        /* set lua handler */
        lua_pushlightuserdata(L, &_L);
        lua_rawget(L, LUA_REGISTRYINDEX);
        lua_pushinteger(L, (intptr_t)env);   /* key: env   TODO */
        lua_pushvalue(L, 2);            /* value: callback */
        lua_rawset(L, -3);
        lua_pop(L, 1);

        tv.tv_sec = timeout / 1000;
        tv.tv_usec = (timeout % 1000) * 1000;
        if ((err = register_timer(EV_PERSIST, &tv,
                    cb_timeout_wrap, env, &env->ev)) < 0) {
            free(env);
        }

        err = 0;
    }
    lua_pushinteger(L, err);
    return 1;
}

/*
 * int blink(gpio, n (times), t (interval))
 */
static int lr_blink(lua_State *L)
{
    int gpio, count, interval;
    int err;
    int n;

    if ((n = lua_gettop(L)) != 3)
        return 0;
    luaL_checkinteger(L, 1);
    luaL_checkinteger(L, 2);
    luaL_checkinteger(L, 3);
    gpio = (int)lua_tointeger(L, 1);
    count = (int)lua_tointeger(L, 2);
    interval = (int)lua_tointeger(L, 3);
    err = gpio_blink(gpio, count, interval);
    lua_pushinteger(L, err);
    return 1;
}

#define USE_BREATH  1

static int pwm(lua_State *L, int flags)
{
    int gpio, count, range, init_data, step, interval;
    int err;
    int n, i;

    if ((n = lua_gettop(L)) != 6)
        return 0;
    for (i = 1; i <= 6; i++)
        luaL_checkinteger(L, i);
    gpio = (int)lua_tointeger(L, 1);
    count = (int)lua_tointeger(L, 2);
    range = (int)lua_tointeger(L, 3);
    init_data = (int)lua_tointeger(L, 4);
    step = (int)lua_tointeger(L, 5);
    interval = (int)lua_tointeger(L, 6);
    if (flags & USE_BREATH)
        err = pwm_breath(gpio, count, range, init_data, step, interval);
    else
        err = pwm_gradual(gpio, count, range, init_data, step, interval);
    lua_pushinteger(L, err);
    return 1;
}

static int lr_pwm(lua_State *L)
{
    return pwm(L, 0);
}

static int lr_breath(lua_State *L)
{
    return pwm(L, USE_BREATH);
}

static int lr_l298n(lua_State *L)
{
    /* FIXME */
    return 0;
}

static int lr_modexec(lua_State *L)
{
    int fd;
    const char *cmd;
    char *buffer;
    int n;
    int err;

    /* TODO  use lua_error() ? */
    if ((n = lua_gettop(L)) != 2)
        return 0;

    fd = (int)luaL_checkinteger(L, 1);
    cmd = luaL_checkstring(L, 2);

    err = -ENOMEM;
    if ((buffer = strdup(cmd)) != NULL) {
        char *s, *cmdexec, *saveptr;

        err = 0;
        for (s = buffer; cmdexec = strtok_r(s, ";", &saveptr); s = NULL)
            err |= module_cmdexec(fd, cmdexec);

        free(buffer);
    }

    lua_pushinteger(L, err);
    return 1;
}

static int lr_gpio_fsel(lua_State *L)
{
    int pin = (int)luaL_checkinteger(L, 1);
    int fsel = (int)luaL_optint(L, 2, BCM2835_GPIO_FSEL_OUTP);
    bcm2835_gpio_fsel(pin, fsel);
    return 0;
}

static int lr_gpio_set(lua_State *L)
{
    int pin = (int)luaL_checkinteger(L, 1);
    int v = (int)luaL_optint(L, 2, 1);
    if (v)
        bcm2835_gpio_set(pin);
    else
        bcm2835_gpio_clr(pin);
    return 0;
}

static int lr_gpio_level(lua_State *L)
{
    int pin = (int)luaL_checkinteger(L, 1);
    int level = (int)bcm2835_gpio_lev(pin);
    lua_pushinteger(L, level);
    return 1;
}

struct signal_env {
    int pin;
    int nr_trig;
    int count;
    int counted;
    struct event *ev;
};

static void cb_gpio_signal_wrap(int fd, short what, void *arg)
{
    struct signal_env *env = arg;

    if (env->count != -1 && env->counted++ >= env->count) {
        eventfd_del(env->ev);
        free(env);
        return;
    }

    /* get gpio signal table */
    lua_pushlightuserdata(_L, &_L);
    lua_rawget(_L, LUA_REGISTRYINDEX);
    lua_pushinteger(_L, env->pin);
    lua_gettable(_L, -2);

    /* call lua handler with one result */
    lua_pushinteger(_L, env->pin);
    lua_pushinteger(_L, bcm2835_gpio_lev(env->pin));
    if (lua_pcall(_L, 2, 1, 0) == 0) {
        int retval = luaL_checkinteger(_L, -1);
        if (retval < 0) {
            eventfd_del(env->ev);
            free(env);
        }
        lua_pop(_L, 1); /* pop result */
    }
}

/* pin, cb, [count], TODO [EDGE] */
static int lr_gpio_signal(lua_State *L)
{
    struct signal_env *env;
    int pin, count;
    int err;

    pin = (int)luaL_checkinteger(L, 1);
    count = (int)luaL_optint(L, 3, -1);
    if (!lua_isfunction(L, 2) || lua_iscfunction(L, 2))
        return 0;
    /* set lua handler */
    lua_pushlightuserdata(L, &_L);
    lua_rawget(L, LUA_REGISTRYINDEX);
    lua_pushvalue(L, 1); /* key: pin */
    lua_pushvalue(L, 2); /* value: callback */
    lua_rawset(L, -3);
    lua_pop(L, 1);

    env = xmalloc(sizeof(*env));
    memset(env, 0, sizeof(*env));
    env->pin = pin;
    env->count = count;
    if ((err = bcm2835_gpio_signal(pin, EDGE_both,
                cb_gpio_signal_wrap, env, &env->ev)) < 0) {
        free(env);
    }
    lua_pushinteger(L, err);
    return 1;
}

static int lr_i2c_init(lua_State *L)
{
    int divider = (int)luaL_optint(L, 1, 64);
    bcm2835_i2c_begin();
    bcm2835_i2c_setClockDivider(divider);
    return 0;
}

static int lr_softpwm_init(lua_State *L)
{
    int cycle_time_us = (int)luaL_optint(L, 1, 2500);
    int step_time_us = (int)luaL_optint(L, 2, 5);
    int err = softpwm_init(cycle_time_us, step_time_us);
    lua_pushinteger(L, err);
    return 1;
}

static int lr_invmpu_init(lua_State *L)
{
    int pin_int = (int)luaL_checkinteger(L, 1);
    int sample_rate = (int)luaL_optint(L, 2, 200);
    int err = invmpu_init(pin_int, sample_rate);
    lua_pushinteger(L, err);
    return 1;
}

static void *alti_dev;

static long get_altitude_from_ultrasonic(unsigned long *timestamp)
{
    /* TODO FIXME */
    return -1;
    if (alti_dev)
        return ultrasonic_get_distance(alti_dev, timestamp);
    return -1;
}

static long get_altitude_from_barometer(unsigned long *timestamp)
{
    return -1;
}

static int lr_pidctrl_init(lua_State *L)
{
    int pin_front, pin_rear, pin_left, pin_right;
    const char *altimeter;
    float angle[5], rate[5], alti[5];
    int i, n;

    pin_front = (int)luaL_checkinteger(L, 1);
    pin_rear  = (int)luaL_checkinteger(L, 2);
    pin_left  = (int)luaL_checkinteger(L, 3);
    pin_right = (int)luaL_checkinteger(L, 4);
    altimeter = luaL_checkstring(L, 8);

    if (altimeter)
        alti_dev = luaenv_getdev(altimeter);

    n = lua_objlen(L, 5);
    for (i = 1; i <= n; i++) {
        lua_rawgeti(L, 5, i);
        angle[i - 1] = luaL_checknumber(L, -1);
    }

    n = lua_objlen(L, 6);
    for (i = 1; i <= n; i++) {
        lua_rawgeti(L, 6, i);
        rate[i - 1] = luaL_checknumber(L, -1);
    }

    n = lua_objlen(L, 7);
    for (i = 1; i <= n; i++) {
        lua_rawgeti(L, 7, i);
        alti[i - 1] = luaL_checknumber(L, -1);
    }

    pidctrl_init(pin_front, pin_rear, pin_left, pin_right,
            strcmp(altimeter, "ultrasonic") == 0 ?
                get_altitude_from_ultrasonic : NULL,
            angle, rate, alti);
    /* TODO */
    return 0;
}

static int lr_stepmotor_new(lua_State *L)
{
    int pin1, pin2, pin3, pin4;
    double step_angle;
    int reduction_ratio, pullin_freq, pullout_freq, flags;
    struct stepmotor_dev **devp;

    pin1 = (int)luaL_checkinteger(L, 1);
    pin2 = (int)luaL_checkinteger(L, 2);
    pin3 = (int)luaL_checkinteger(L, 3);
    pin4 = (int)luaL_checkinteger(L, 4);
    step_angle = luaL_checknumber(L, 5);
    reduction_ratio = (int)luaL_checkinteger(L, 6);
    pullin_freq = (int)luaL_checkinteger(L, 7);
    pullout_freq = (int)luaL_checkinteger(L, 8);
    flags = (int)luaL_checkinteger(L, 9);

    devp = lua_newuserdata(L, sizeof(struct stepmotor_dev *));
    *devp = stepmotor_new(pin1, pin2, pin3, pin4, step_angle,
                reduction_ratio, pullin_freq, pullout_freq, flags);
    if (*devp == NULL) {
        /* TODO:  return 0; */
        luaL_error(L, "stepmotor_new() error\n");
    }
    return 1;
}

static int lr_stepmotor_del(lua_State *L)
{
    struct stepmotor_dev **devp = lua_touserdata(L, 1);
    stepmotor_del(*devp);
    return 0;
}

static int cb_stepmotor_done_wrap(struct stepmotor_dev *dev, void *opaque)
{
    /* get table */
    lua_pushlightuserdata(_L, &_L);
    lua_rawget(_L, LUA_REGISTRYINDEX);
    lua_pushlightuserdata(_L, dev); /* key */
    lua_gettable(_L, -2);

    /* call lua handler with one arg, one result */
    lua_pushinteger(_L, dev->angle);
    if (lua_pcall(_L, 1, 1, 0) == 0) {
        /* TODO */
        lua_pop(_L, 1);
    }
    return 0;
}

static int lr_stepmotor(lua_State *L)
{
    struct stepmotor_dev **devp = lua_touserdata(L, 1);
    double angle = luaL_checkinteger(L, 2);
    int delay = (int)luaL_checkinteger(L, 3);
    int err;

    if (!lua_isfunction(L, 4) || lua_iscfunction(L, 4))
        return 0;

    /* set lua handler */
    lua_pushlightuserdata(L, &_L);
    lua_rawget(L, LUA_REGISTRYINDEX);
    lua_pushlightuserdata(L, *devp); /* key: dev */
    lua_pushvalue(L, 4);             /* value: callback */
    lua_rawset(L, -3);
    lua_pop(L, 1);

    err = stepmotor(*devp, angle, delay, cb_stepmotor_done_wrap, NULL);
    if (err < 0) {
        /* TODO */
    }
    lua_pushinteger(L, err);
    return 1;
}

static int lr_l298n_new(lua_State *L)
{
    int ena, enb, in1, in2, in3, in4;
    int max_speed, range, pwm_div;
    struct l298n_dev **devp;

    ena = (int)luaL_checkinteger(L, 1);
    enb = (int)luaL_checkinteger(L, 2);
    in1 = (int)luaL_checkinteger(L, 3);
    in2 = (int)luaL_checkinteger(L, 4);
    in3 = (int)luaL_checkinteger(L, 5);
    in4 = (int)luaL_checkinteger(L, 6);
    max_speed = (int)luaL_checkinteger(L, 7);
    range = (int)luaL_checkinteger(L, 8);
    pwm_div = (int)luaL_checkinteger(L, 9);

    devp = lua_newuserdata(L, sizeof(struct l298n_dev *));
    *devp = l298n_new(ena, enb, in1, in2, in3, in4, max_speed,
                range, pwm_div);
    if (*devp == NULL) {
        /* TODO:  return 0; */
        luaL_error(L, "l298n_new() error\n");
    }
    return 1;
}

static int lr_l298n_del(lua_State *L)
{
    struct l298n_dev **devp = lua_touserdata(L, 1);
    l298n_del(*devp);
    return 0;
}

static int lr_l298n_lup(lua_State *L)
{
    struct l298n_dev **devp = lua_touserdata(L, 1);
    l298n_lup(*devp);
    return 0;
}

static int lr_l298n_ldown(lua_State *L)
{
    struct l298n_dev **devp = lua_touserdata(L, 1);
    l298n_ldown(*devp);
    return 0;
}

static int lr_l298n_lbrake(lua_State *L)
{
    struct l298n_dev **devp = lua_touserdata(L, 1);
    l298n_lbrake(*devp);
    return 0;
}

static int lr_l298n_lspeedup(lua_State *L)
{
    struct l298n_dev **devp = lua_touserdata(L, 1);
    l298n_lspeedup(*devp);
    return 0;
}

static int lr_l298n_lspeeddown(lua_State *L)
{
    struct l298n_dev **devp = lua_touserdata(L, 1);
    l298n_lspeeddown(*devp);
    return 0;
}

static int lr_l298n_rup(lua_State *L)
{
    struct l298n_dev **devp = lua_touserdata(L, 1);
    l298n_rup(*devp);
    return 0;
}

static int lr_l298n_rdown(lua_State *L)
{
    struct l298n_dev **devp = lua_touserdata(L, 1);
    l298n_rdown(*devp);
    return 0;
}

static int lr_l298n_rbrake(lua_State *L)
{
    struct l298n_dev **devp = lua_touserdata(L, 1);
    l298n_rbrake(*devp);
    return 0;
}

static int lr_l298n_rspeedup(lua_State *L)
{
    struct l298n_dev **devp = lua_touserdata(L, 1);
    l298n_rspeedup(*devp);
    return 0;
}

static int lr_l298n_rspeeddown(lua_State *L)
{
    struct l298n_dev **devp = lua_touserdata(L, 1);
    l298n_rspeeddown(*devp);
    return 0;
}

static int lr_l298n_set(lua_State *L)
{
    struct l298n_dev **devp = lua_touserdata(L, 1);
    int left_speed = (int)luaL_checkinteger(L, 2);
    int right_speed = (int)luaL_checkinteger(L, 3);
    l298n_set(*devp, left_speed, right_speed);
    return 0;
}

static int lr_l298n_get(lua_State *L)
{
    struct l298n_dev **devp = lua_touserdata(L, 1);
    int left_speed = 0, right_speed = 0;
    l298n_get(*devp, &left_speed, &right_speed);
    lua_pushinteger(L, left_speed);
    lua_pushinteger(L, right_speed);
    return 2;
}

static int lr_l298n_change(lua_State *L)
{
    struct l298n_dev **devp = lua_touserdata(L, 1);
    int left = (int)luaL_checkinteger(L, 2);
    int right = (int)luaL_checkinteger(L, 3);
    l298n_change(*devp, left, right);
    return 0;
}

static int lr_ultrasonic_new(lua_State *L)
{
    int pin_trig, pin_echo;
    int trig_time;
    struct ultrasonic_dev **devp;

    pin_trig = (int)luaL_checkinteger(L, 1);
    pin_echo = (int)luaL_checkinteger(L, 2);
    trig_time = (int)luaL_checkinteger(L, 3);

    devp = lua_newuserdata(L, sizeof(struct ultrasonic_dev *));
    *devp = ultrasonic_new(pin_trig, pin_echo, trig_time);
    if (*devp == NULL) {
        luaL_error(L, "ultrasonic_new() error\n");
        return 0;
    }
    return 1;
}

static int lr_ultrasonic_del(lua_State *L)
{
    struct ultrasonic_dev **devp = lua_touserdata(L, 1);
    ultrasonic_del(*devp);
    return 0;
}

static int cb_ultrasonic_wrap(struct ultrasonic_dev *dev,
                                double distance, void *opaque)
{
    int retval = 0;
    /* get table */
    lua_pushlightuserdata(_L, &_L);
    lua_rawget(_L, LUA_REGISTRYINDEX);
    lua_pushlightuserdata(_L, dev);
    lua_gettable(_L, -2);

    /* call lua handler with one arg, one result */
    lua_pushnumber(_L, distance);
    if (lua_pcall(_L, 1, 1, 0) == 0) {
        retval = luaL_checkinteger(_L, -1);
        lua_pop(_L, 1);
    }
    return retval;
}

/* dev, cb, count, interval */
static int lr_ultrasonic_scope(lua_State *L)
{
    struct ultrasonic_dev **devp = lua_touserdata(L, 1);
    int count, interval;
    int err;

    count = (int)luaL_optint(L, 3, -1);
    interval = (int)luaL_optint(L, 4, 500/* 0.5s */);
    if ((!lua_isfunction(L, 2) || lua_iscfunction(L, 2)) && interval > 0)
        return 0;

    /* set lua handler */
    lua_pushlightuserdata(L, &_L);
    lua_rawget(L, LUA_REGISTRYINDEX);
    lua_pushlightuserdata(L, *devp); /* key: dev */
    lua_pushvalue(L, 2);             /* value: callback */
    lua_rawset(L, -3);
    lua_pop(L, 1);

    err = ultrasonic_scope(*devp, count, interval, cb_ultrasonic_wrap, NULL);
    if (err < 0) {
        /* TODO */
    }
    lua_pushinteger(L, err);
    return 1;
}

static int lr_ultrasonic_is_busy(lua_State *L)
{
    struct ultrasonic_dev **devp = lua_touserdata(L, 1);
    lua_pushboolean(L, (int)ultrasonic_is_busy(*devp));
    return 1;
}

static int lr_ultrasonic_get_distance(lua_State *L)
{
    struct ultrasonic_dev **devp = lua_touserdata(L, 1);
    unsigned long timestamp;
    float distance;
    distance = ultrasonic_get_distance(*devp, &timestamp);
    lua_pushnumber(L, distance);
    lua_pushnumber(L, timestamp);
    return 2;
}

/*
 * tank
 */
static int lr_tank_new(lua_State *L)
{
    int pin;
    struct tank_dev **devp;

    pin = (int)luaL_checkinteger(L, 1);

    devp = lua_newuserdata(L, sizeof(struct tank_dev *));
    *devp = tank_new(pin);
    if (*devp == NULL) {
        /* TODO:  return 0; */
        luaL_error(L, "tank_new() error\n");
    }
    return 1;
}

static int lr_tank_del(lua_State *L)
{
    struct tank_dev **devp = lua_touserdata(L, 1);
    tank_del(*devp);
    return 0;
}

static int lr_tank_sdown(lua_State *L)
{
    struct tank_dev **devp = lua_touserdata(L, 1);
    tank_sdown(*devp);
    return 0;
}

static int lr_tank_sup(lua_State *L)
{
    struct tank_dev **devp = lua_touserdata(L, 1);
    tank_sup(*devp);
    return 0;
}

static int lr_tank_brake(lua_State *L)
{
    struct tank_dev **devp = lua_touserdata(L, 1);
    tank_brake(*devp);
    return 0;
}

static int lr_tank_fwd(lua_State *L)
{
    struct tank_dev **devp = lua_touserdata(L, 1);
    tank_fwd(*devp);
    return 0;
}

static int lr_tank_rev(lua_State *L)
{
    struct tank_dev **devp = lua_touserdata(L, 1);
    tank_rev(*devp);
    return 0;
}

static int lr_tank_left(lua_State *L)
{
    struct tank_dev **devp = lua_touserdata(L, 1);
    tank_left(*devp);
    return 0;
}

static int lr_tank_right(lua_State *L)
{
    struct tank_dev **devp = lua_touserdata(L, 1);
    tank_right(*devp);
    return 0;
}

static int lr_tank_turret_left(lua_State *L)
{
    struct tank_dev **devp = lua_touserdata(L, 1);
    tank_turret_left(*devp);
    return 0;
}

static int lr_tank_turret_right(lua_State *L)
{
    struct tank_dev **devp = lua_touserdata(L, 1);
    tank_turret_right(*devp);
    return 0;
}

static int lr_tank_turret_elev(lua_State *L)
{
    struct tank_dev **devp = lua_touserdata(L, 1);
    tank_turret_elev(*devp);
    return 0;
}

static int lr_tank_fire(lua_State *L)
{
    struct tank_dev **devp = lua_touserdata(L, 1);
    tank_fire(*devp);
    return 0;
}

static const luaL_Reg luaraspd_lib[] = {
    { "timeout", lr_timeout },
    { "blink",   lr_blink   },
    { "pwm",     lr_pwm     },
    { "breath",  lr_breath  },
    { "l298n",   lr_l298n   },
    { "modexec", lr_modexec },
    { "gpio_fsel",   lr_gpio_fsel   },
    { "gpio_set",    lr_gpio_set    },
    { "gpio_level",  lr_gpio_level  },
    { "gpio_signal", lr_gpio_signal },

    /* misc */
    { "i2c_init",     lr_i2c_init     },
    { "softpwm_init", lr_softpwm_init },
    { "invmpu_init",  lr_invmpu_init  },
    { "pidctrl_init", lr_pidctrl_init },

    /* stepmotor */
    { "stepmotor_new", lr_stepmotor_new },
    { "stepmotor_del", lr_stepmotor_del },
    { "stepmotor",     lr_stepmotor     },

    /* l298n */
    { "l298n_new", lr_l298n_new },
    { "l298n_del", lr_l298n_del },
    { "l298n_lup", lr_l298n_lup },
    { "l298n_ldown", lr_l298n_ldown },
    { "l298n_lbrake", lr_l298n_lbrake },
    { "l298n_lspeedup", lr_l298n_lspeedup },
    { "l298n_lspeeddown", lr_l298n_lspeeddown },
    { "l298n_rup", lr_l298n_rup },
    { "l298n_rdown", lr_l298n_rdown },
    { "l298n_rbrake", lr_l298n_rbrake },
    { "l298n_rspeedup", lr_l298n_rspeedup },
    { "l298n_rspeeddown", lr_l298n_rspeeddown },
    { "l298n_set", lr_l298n_set },
    { "l298n_get", lr_l298n_get },
    { "l298n_change", lr_l298n_change },

    /* ultrasonic */
    { "ultrasonic_new",     lr_ultrasonic_new     },
    { "ultrasonic_del",     lr_ultrasonic_del     },
    { "ultrasonic_scope",   lr_ultrasonic_scope   },
    { "ultrasonic_is_busy", lr_ultrasonic_is_busy },
    { "ultrasonic_get_distance", lr_ultrasonic_get_distance },


    /* tank */
    { "tank_new",   lr_tank_new   },
    { "tank_del",   lr_tank_del   },
    { "tank_sdown", lr_tank_sdown },
    { "tank_sup",   lr_tank_sup   },
    { "tank_brake", lr_tank_brake },
    { "tank_fwd",   lr_tank_fwd   },
    { "tank_rev",   lr_tank_rev   },
    { "tank_left",  lr_tank_left  },
    { "tank_right", lr_tank_right },
    { "tank_turret_left",  lr_tank_turret_left  },
    { "tank_turret_right", lr_tank_turret_right },
    { "tank_turret_elev",  lr_tank_turret_elev  },
    { "tank_fire",  lr_tank_fire  },

    { NULL, NULL }
};

static int luaopen_luaraspd(lua_State *L)
{
    /*
     * only exported by lua 5.2 and above
     * luaL_newlib(L, luaraspd_lib);
     */
#if LUA_VERSION_NUM >= 502
    lua_newtable(L);
    luaL_setfuncs(L, luaraspd_lib, 0);
#else
    luaL_register(L, "luaraspd", luaraspd_lib);
#endif

    /* gpio signal table */
    lua_pushlightuserdata(L, &_L); /* key */
    lua_newtable(L); /* value */
    lua_rawset(L, LUA_REGISTRYINDEX);

    return 1;
}

/*
 * lua for raspd
 */

int luaenv_getconf_int(const char *table, const char *key, int *v)
{
    lua_getglobal(_L, table);
    if (!lua_istable(_L, -1))
        return -EINVAL;
    lua_getfield(_L, -1, key);
    if (!lua_isnumber(_L, -1))
        return -ENOSPC;
    *v = (int)lua_tonumber(_L, -1);
    lua_pop(_L, 1);
    return 0;
}

/*
 * XXX: pop it after used
 */
int luaenv_getconf_str(const char *table, const char *key, const char **v)
{
    lua_getglobal(_L, table);
    if (!lua_istable(_L, -1))
        return -EINVAL;
    lua_getfield(_L, -1, key);
    if (!lua_isstring(_L, -1))
        return -ENOSPC;
    *v = lua_tostring(_L, -1);
    /* donot pop */
    return 0;
}

void luaenv_pop(int n)
{
    lua_pop(_L, n);
}

/*
 * invoke a lua function
 *      fmt: d(double), i(int), s(string), :(separator)
 *
 * XXX: lua_pop(L, nres) should be called after this
 */
int luaenv_call_va(const char *func, const char *fmt, ...)
{
    va_list ap;
    int narg, nres;

    va_start(ap, fmt);

    lua_getglobal(_L, func);

    /* push args */
    for (narg = 0; *fmt; narg++) {
        luaL_checkstack(_L, 1, "too many arguments");
        switch (*fmt++) {
        case 'd': lua_pushnumber(_L, va_arg(ap, double)); break;
        case 'i': lua_pushinteger(_L, va_arg(ap, int)); break;
        case 's': lua_pushstring(_L, va_arg(ap, char *)); break;
        case ':': goto endargs;
        default:
            return -EINVAL;
        }
    }

  endargs:

    /* do call */
    nres = strlen(fmt);
    if (lua_pcall(_L, narg, nres, 0) != 0)
        return -EFAULT;

    /* fetch results */
    for (nres = -nres; *fmt; nres++) {
        switch (*fmt++) {
        case 'd':
            if (!lua_isnumber(_L, nres))
                return -ENOENT;
            *va_arg(ap, double *) = lua_tonumber(_L, nres);
            break;
        case 'i':
            if (!lua_isnumber(_L, nres))
                return -ENOENT;
            *va_arg(ap, int *) = lua_tonumber(_L, nres);
            break;
        case 's':
            if (!lua_isstring(_L, nres))
                return -ENOENT;
            *va_arg(ap, const char **) = lua_tostring(_L, nres);
            break;
        default:
            return -EINVAL;
        }
    }

    va_end(ap);
    return 0;
}

int luaenv_run_file(const char *file)
{
    if (luaL_loadfile(_L, file) != 0)
        return -EBADF;
    if (lua_pcall(_L, 0, 0, 0) != 0 && !lua_isnil(_L, -1))
        return -EFAULT;
    return 0;
}

void *luaenv_getdev(const char *name)
{
    void **devp;
    void *dev = NULL;
    /* call __DEV lua function */
    lua_getglobal(_L, "__DEV");
    if (!lua_isfunction(_L, -1) || lua_iscfunction(_L, -1))
        return NULL;
    lua_pushstring(_L, name);
    if (lua_pcall(_L, 1, 1, 0) != 0)
        return NULL;
    if (devp = lua_touserdata(_L, -1))
        dev = *devp;
    lua_pop(_L, 1);
    return dev;
}

int luaenv_init(void)
{
    _L = luaL_newstate();
    luaL_openlibs(_L);

    /* register luaraspd lib */
    /*
     * only exported by lua 5.2 and above
     * luaL_requiref(L, "luaraspd", luaopen_luaraspd, 1);
     * lua_pop(L, 1);
     */
    lua_pushcfunction(_L, luaopen_luaraspd);
    lua_pushstring(_L, "luaraspd");
    lua_call(_L, 1, 0);

    return 0;
}

void luaenv_exit(void)
{
    if (_L)
        lua_close(_L);
}
