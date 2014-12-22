#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <errno.h>

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

/* 1-54: used by gpio */
#define ULTRASONIC_INDEX    64
#define STEPMOTOR_INDEX     96

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

static void free_signal_env(struct signal_env *env)
{
    free(env);
}

static void cb_gpio_signal_wrap(int fd, short what, void *arg)
{
    struct signal_env *env = arg;

    if (env->count != -1 && env->counted++ >= env->count) {
        eventfd_del(env->ev);
        free_signal_env(env);
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
            free_signal_env(env);
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
    count = (int)luaL_optint(L, 3, 1);
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
        free_signal_env(env);
    }
    lua_pushinteger(L, err);
    return 1;
}

static int ultrasonic_callback_wrap(double distance, void *opaque)
{
    int retval = 0;

    /* get table */
    lua_pushlightuserdata(_L, &_L);
    lua_rawget(_L, LUA_REGISTRYINDEX);
    lua_pushinteger(_L, ULTRASONIC_INDEX);
    lua_gettable(_L, -2);

    /* call lua handler with one result */
    lua_pushinteger(_L, distance);
    if (lua_pcall(_L, 1, 1, 0) == 0) {
        retval = luaL_checkinteger(_L, -1);
        lua_pop(_L, 1);
    }
    return retval;
}

/* cb, [count], [interval] */
static int lr_ultrasonic_scope(lua_State *L)
{
    int count, interval;
    int err;

    count = (int)luaL_optint(L, 2, -1);
    if ((!lua_isfunction(L, 1) || lua_iscfunction(L, 1)) && count > 0)
        return 0;
    interval = (int)luaL_optint(L, 3, 2000); /* 2s */
    /* set lua handler */
    lua_pushlightuserdata(L, &_L);
    lua_rawget(L, LUA_REGISTRYINDEX);
    lua_pushinteger(L, ULTRASONIC_INDEX); /* key */
    lua_pushvalue(L, 1); /* value: callback */
    lua_rawset(L, -3);
    lua_pop(L, 1);

    err = ultrasonic_scope(count, interval, ultrasonic_callback_wrap, NULL);
    if (err < 0) {
        /* TODO  pop */
    }
    lua_pushinteger(L, err);
    return 1;
}

static int lr_ultrasonic_is_using(lua_State *L)
{
    lua_pushboolean(L, (int)ultrasonic_is_using());
    return 1;
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
    step_angle = luaL_checkinteger(L, 5);
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
    lua_pushlightuserdata(L, &L);
    lua_rawget(L, LUA_REGISTRYINDEX);
    lua_pushinteger(L, STEPMOTOR_INDEX);
    lua_gettable(L, -2);

    /* call lua handler with one arg, one result */
    lua_pushinteger(L, dev->angle);
    if (lua_pcall(L, 1, 1, 0) == 0) {
        /* TODO */
        lua_pop(L, 1);
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
    lua_pushlightuserdata(L, &L);
    lua_rawget(L, LUA_REGISTRYINDEX);
    lua_pushinteger(L, STEPMOTOR_INDEX);
    lua_pushvalue(L, 4); /* value: callback */
    lua_rawset(L, -3);
    lua_pop(L, 1);

    err = stepmotor(*devp, angle, delay, cb_stepmotor_done_wrap, NULL);
    if (err < 0) {
        /* TODO */
    }
    lua_pushinteger(L, err);
    return 1;
}

static const luaL_Reg luaraspd_lib[] = {
    { "blink",   lr_blink   },
    { "pwm",     lr_pwm     },
    { "breath",  lr_breath  },
    { "l298n",   lr_l298n   },
    { "modexec", lr_modexec },
    { "gpio_fsel",   lr_gpio_fsel   },
    { "gpio_set",    lr_gpio_set    },
    { "gpio_level",  lr_gpio_level  },
    { "gpio_signal", lr_gpio_signal },
    { "ultrasonic_scope",    lr_ultrasonic_scope    },
    { "ultrasonic_is_using", lr_ultrasonic_is_using },

    /* stepmotor */
    { "stepmotor_new", lr_stepmotor_new },
    { "stepmotor_del", lr_stepmotor_del },
    { "stepmotor",     lr_stepmotor     },

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
