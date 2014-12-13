#include <stdio.h>
#include <errno.h>
#include <getopt.h>

#include "module.h"
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
