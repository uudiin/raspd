#ifndef __MODULE_H__
#define __MODULE_H__

#include <queue.h>
#include <tree.h>

struct module {
    const char *name;
    int (*early_init)(void); /* run after bcm2835_init */
    int (*init)(void);       /* run after event_init & eve loop */
    void (*exit)(void);
    int (*main)(int wfd, int argc, char *argv[]);
    RB_ENTRY(module) node;
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

#define DEFINE_MODULE_INIT(mod)                     \
    static struct module __module_ ## mod = {       \
        .name = # mod,                              \
        .init = mod ## _init,                       \
        .main = mod ## _main                        \
    };                                              \
    static __init void __reg_moudle_ ## mod(void) { \
        register_module(&__module_ ## mod);         \
    }

#endif /* __MODULE_H__ */
