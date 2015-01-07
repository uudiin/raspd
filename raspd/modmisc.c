#include <stdio.h>
#include <errno.h>
#include <getopt.h>

#include "module.h"
#include "event.h"
#include "luaenv.h"

static int automatic_main(int fd, int argc, char *argv[])
{
    static struct option options[] = {
        { "start", no_argument, NULL, 's' },
        { "stop",  no_argument, NULL, 'e' },
        { 0, 0, 0, 0 }
    };
    int c;
    int start = 1;
    int err = 0;

    while ((c = getopt_long(argc, argv, "se", options, NULL)) != -1) {
        switch(c) {
        case 's': break;
        case 'e': start = 0; break;
        default:
            return 1;
        }
    }

    if (start) {
        const char *luaf = NULL;

        luaenv_getconf_str("_G", "automatic", &luaf);
        if (luaf) {
            err = luaenv_call_va(luaf, "");
            if (err < 0)
                fprintf(stderr, "luaenv_call_va(%s), err = %d\n", luaf, err);
            luaenv_pop(1);
        }
    }
    return (err == 0) ? 0 : 1;
}

static int automatic_init(void)
{
    return 0;
}

static struct module __module_automatic = {
    .name = "automatic",
    .init = automatic_init,
    .main = automatic_main
};

static __init void __reg_module_automatic(void)
{
    register_module(&__module_automatic);
}

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
    err = luaenv_call_va(argv[1], "");
    if (err < 0)
        fprintf(stderr, "luaenv_call_va(%s), err = %d\n", argv[1], err);
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
}

DEFINE_MODULE_INIT(luamisc);
