#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <getopt.h>
#include <errno.h>

#include <queue.h>
#include <xmalloc.h>
#include <unix.h>

#include "module.h"

static TAILQ_HEAD(module_list, module) list_head = TAILQ_HEAD_INITIALIZER(list_head);

void register_module(struct module *m)
{
    TAILQ_INSERT_TAIL(&list_head, m, list);
}

static struct module *find_module(const char *name)
{
    struct module *m;

    TAILQ_FOREACH(m, &list_head, list) {
        if (strcmp(m->name, name) == 0)
            return m;
    }

    return NULL;
}

int module_execv(int argc, char *argv[])
{
    struct module *m;

    if ((m = find_module(argv[0])) == NULL)
        return -ENOENT;

    optind = 0; /* FIXME  needed? */
    return m->main(argc, argv);
}

int module_execl(const char *modname, /*const char *arg0, ..., 0,*/ ...)
{
    va_list ap;
    char *p;
    int i;
    int argc = 1;
    char **argv;
    int err;

    va_start(ap, modname);
    while (p = va_arg(ap, char *))
        argc += 1;

    va_start(ap, modname);
    argv = (char **)xmalloc((argc + 1) * sizeof(char *));
    argv[0] = (char *)modname;
    for (i = 1; p = va_arg(ap, char *); i++)
        argv[i] = p;
    argv[i] = NULL;

    va_end(ap);

    err = module_execv(argc, argv);

    free(argv);
    return err;
}

int module_cmdexec(const char *cmdexec)
{
    char **cmd_argv;
    int cmd_argc = 0;
    int err;

    cmd_argv = cmdline_split(cmdexec, &cmd_argc);
    if (cmd_argv == NULL || cmd_argc == 0)
        return -EINVAL;

    err = module_execv(cmd_argc, cmd_argv);

    free_cmd_argv(cmd_argv);
    return err;
}
