#include <stdio.h>
#include <errno.h>

#include <queue.h>

static TAILQ_HEAD(module_list, module) list_head = TAILQ_HEAD_INITIALIZER(list_head);

void register_module(struct module *m)
{
    TAILQ_INSERT_TAIL(&list_head, m, list);
}

static struct module *find_module(const char *name)
{
    struct module *retval;

    TAILQ_FOREACH(retval, &list_head, list) {
        if (strcmp(retval->name, name) == 0)
            return retval;
    }

    return NULL;
}

int invoke_event(const char *name, const char *cmd)
{
    struct module *m;

    m = find_module(name);
    if (m == NULL)
        return -ENOENT;

    /* do event callback */
    m->event(cmd);

    return 0;
}
