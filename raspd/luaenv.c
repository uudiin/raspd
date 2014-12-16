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

#include "luaenv.h"

static lua_State *L;

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
    lua_pushlightuserdata(L, &L);
    lua_rawget(L, LUA_REGISTRYINDEX);
    lua_pushinteger(L, env->pin);
    lua_gettable(L, -2);

    /* call lua handler with one result */
    lua_pushinteger(L, env->pin);
    if (lua_pcall(L, 0, 1, 0) == 0) {
        int retval = luaL_checkinteger(L, -1);
        if (retval < 0) {
            eventfd_del(env->ev);
            free_signal_env(env);
        }
        lua_pop(L, 1); /* pop result */
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
    lua_pushlightuserdata(L, &L);
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
    lua_pushlightuserdata(L, &L); /* key */
    lua_newtable(L); /* value */
    lua_rawset(L, LUA_REGISTRYINDEX);

    return 1;
}

/*
 * lua for raspd
 */

int luaenv_getconf_int(const char *table, const char *key, int *v)
{
    lua_getglobal(L, table);
    if (!lua_istable(L, -1))
        return -EINVAL;
    lua_getfield(L, -1, key);
    if (!lua_isnumber(L, -1))
        return -ENOSPC;
    *v = (int)lua_tonumber(L, -1);
    lua_pop(L, 1);
    return 0;
}

/*
 * XXX: pop it after used
 */
int luaenv_getconf_str(const char *table, const char *key, const char **v)
{
    lua_getglobal(L, table);
    if (!lua_istable(L, -1))
        return -EINVAL;
    lua_getfield(L, -1, key);
    if (!lua_isstring(L, -1))
        return -ENOSPC;
    *v = lua_tostring(L, -1);
    /* donot pop */
    return 0;
}

void luaenv_pop(int n)
{
    lua_pop(L, n);
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

    lua_getglobal(L, func);

    /* push args */
    for (narg = 0; *fmt; narg++) {
        luaL_checkstack(L, 1, "too many arguments");
        switch (*fmt++) {
        case 'd': lua_pushnumber(L, va_arg(ap, double)); break;
        case 'i': lua_pushinteger(L, va_arg(ap, int)); break;
        case 's': lua_pushstring(L, va_arg(ap, char *)); break;
        case ':': goto endargs;
        default:
            return -EINVAL;
        }
    }

  endargs:

    /* do call */
    nres = strlen(fmt);
    if (lua_pcall(L, narg, nres, 0) != 0)
        return -EFAULT;

    /* fetch results */
    for (nres = -nres; *fmt; nres++) {
        switch (*fmt++) {
        case 'd':
            if (!lua_isnumber(L, nres))
                return -ENOENT;
            *va_arg(ap, double *) = lua_tonumber(L, nres);
            break;
        case 'i':
            if (!lua_isnumber(L, nres))
                return -ENOENT;
            *va_arg(ap, int *) = lua_tonumber(L, nres);
            break;
        case 's':
            if (!lua_isstring(L, nres))
                return -ENOENT;
            *va_arg(ap, const char **) = lua_tostring(L, nres);
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
    if (luaL_loadfile(L, file) != 0)
        return -EBADF;
    if (lua_pcall(L, 0, 0, 0) != 0 && !lua_isnil(L, -1))
        return -EFAULT;
    return 0;
}

int luaenv_init(void)
{
    L = luaL_newstate();
    luaL_openlibs(L);

    /* register luaraspd lib */
    /*
     * only exported by lua 5.2 and above
     * luaL_requiref(L, "luaraspd", luaopen_luaraspd, 1);
     * lua_pop(L, 1);
     */
    lua_pushcfunction(L, luaopen_luaraspd);
    lua_pushstring(L, "luaraspd");
    lua_call(L, 1, 0);

    return 0;
}

void luaenv_exit(void)
{
    if (L)
        lua_close(L);
}
