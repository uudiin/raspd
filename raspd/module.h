#ifndef __MODULE_H__
#define __MODULE_H__

#include <queue.h>

struct module {
    const char *name;
    int (*init)(void);
    void (*exit)(void);
    /*int (*lua_config)(lua_State *L);*/
    int (*main)(int wfd, int argc, char *argv[]);
    TAILQ_ENTRY(module) list;
};

#define __init  __attribute__((constructor))
#define __exit  __attribute__((destructor))

void register_module(struct module *m);
int foreach_module(int (*fn)(struct module *m, void *opaque), void *opaque);
int module_execv(int wfd, int argc, char *argv[]);
int module_execl(int wfd, const char *modname, /*const char *arg0, ..., 0,*/ ...);
int module_cmdexec(int wfd, const char *cmdexec);

#define DEFINE_MODULE(mod)                          \
    static struct module __module_ ## mod = {       \
        .name = # mod,                              \
        .main = mod ## _main                        \
    };                                              \
    static __init void __reg_moudle_ ## mod(void) { \
        register_module(&__module_ ## mod);         \
    }

#endif /* __MODULE_H__ */
