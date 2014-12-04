#include <stdio.h>
#include <stdlib.h>
#include <errno.h>

#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>

#include <bcm2835.h>

#include "module.h"
#include "event.h"

static lua_State *L;

/*
 * int blink(gpio, n (times), t (interval))
 */
static int lr_blink(lua_State *L)
{
    module_execl();
}

static int lr_modexec(lua_State *L)
{
}

static const luaL_Reg luaraspd_funcs[] = {
    { "blink",   lr_blink   },
    { "pwm",     lr_pwm     },
    { "l298n",   lr_l298n   },
    { "modexec", lr_modexec },
    { NULL, NULL }
};

int luaopen_luaraspd(lua_State *L)
{
    luaL_newlib(L, luaraspd_funcs);
    return 1;
}

/* lua for raspd */

int luaraspd_init(void)
{
    L = luaL_newstate();
    luaL_openlibs(L);

    /* register luaraspd lib */
    luaL_requiref(L, "luaraspd", luaopen_luaraspd, 1/* global */);
    lua_pop(L, 1);

    return 0;
}

void luaraspd_exit(void)
{
    lua_close(L);
}
