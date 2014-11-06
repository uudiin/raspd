#ifndef __MODULE_H__
#define __MODULE_H__

#include <queue.h>

struct module {
    const char *name;
    void (*event)(char *cmd);
    TAILQ_ENTRY(module) list;
};

#define __init  __attribute__((constructor))
#define __exit  __attribute__((destructor))

void register_module(struct module *m);
int invoke_event(const char *name, char *cmd);

#endif /* __MODULE_H__ */
