#include <stdio.h>
#include <errno.h>
#include <getopt.h>

#include "module.h"
#include "event.h"
#include "luaenv.h"

/*
 * exit module
 */
static int exit_main(int fd, int argc, char *argv[])
{
    /* TODO  retval */
    return rasp_event_loopexit();
}

DEFINE_MODULE(exit);

/*
 * lua misc module
 */
static int luamisc_main(int fd, int argc, char *argv[])
{
    int err;
    if (argc < 2)
        return 1;
    err = luaenv_call_va(argv[1], "");
    if (err < 0) {
        fprintf(stderr, "luaenv_call_va(%s), err = %d\n", argv[1], err);
        return 1;
    }
    return 0;
}

static int luamisc_init(void)
{
    const char *lua_file = NULL;
    int err;
    luaenv_getconf_str("_G", "steer", &lua_file);
    if (lua_file) {
        if ((err = luaenv_run_file(lua_file)) < 0)
            fprintf(stderr, "luaenv_run_fle(%s), err = %d\n", lua_file, err);
        luaenv_pop(1);
    }
    return 0;
}

DEFINE_MODULE_INIT(luamisc);
