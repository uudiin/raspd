#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <getopt.h>
#include <errno.h>
#include <unistd.h>

#include <queue.h>
#include <tree.h>
#include <xmalloc.h>
#include <unix.h>

#include "module.h"

static RB_HEAD(modtree, module) modroot = RB_INITIALIZER(0);
static int mod_comp(struct module *m1, struct module *m2)
{
    return strcmp(m1->name, m2->name);
}
RB_GENERATE_STATIC(modtree, module, node, mod_comp);


void register_module(struct module *m)
{
    RB_INSERT(modtree, &modroot, m);
}

static struct module *find_module(const char *name)
{
    struct module *m;
    RB_FOREACH(m, modtree, &modroot) {
        if (strcmp(m->name, name) == 0)
            return m;
    }
    return NULL;
}

int foreach_module(int (*fn)(struct module *m, void *opaque), void *opaque)
{
    struct module *m;
    int err = 0;

    RB_FOREACH(m, modtree, &modroot) {
        if ((err = fn(m, opaque)) < 0)
            break;
    }
    return err;
}

int module_execv(int wfd, int argc, char *argv[])
{
    struct module *m;

    if ((m = find_module(argv[0])) == NULL)
        m = find_module("luamisc");
    if (m == NULL)
        return -ENOENT;

    /* FIXME  needed? */
    optind = 1;

    if (m->main == NULL)
	return 0;

    return m->main(wfd, argc, argv);
}

int module_execl(int wfd, const char *modname, /*const char *arg0, ..., 0,*/ ...)
{
    va_list ap;
    char *p;
    int i;
    int argc = 1;
    char **argv;
    int err;

    va_start(ap, modname);
    while ((p = va_arg(ap, char *)) != NULL)
        argc += 1;

    va_start(ap, modname);
    argv = (char **)xmalloc((argc + 1) * sizeof(char *));
    argv[0] = (char *)modname;
    for (i = 1; (p = va_arg(ap, char *)) != NULL; i++)
        argv[i] = p;
    argv[i] = NULL;

    va_end(ap);

    err = module_execv(wfd, argc, argv);

    free(argv);
    return err;
}

int module_cmdexec(int wfd, const char *cmdexec)
{
    char **cmd_argv;
    int cmd_argc = 0;
    int err;

    cmd_argv = cmdline_split(cmdexec, &cmd_argc);
    if (cmd_argv == NULL || cmd_argc == 0)
        return -EINVAL;

    err = module_execv(wfd, cmd_argc, cmd_argv);

    free_cmd_argv(cmd_argv);
    return err;
}

/*********************************************************/

int module_main(int fd, int argc, char *argv[])
{
    struct module *m;
    char buffer[128];

    RB_FOREACH(m, modtree, &modroot) {
        int n = snprintf(buffer, sizeof(buffer), "--> %s\n", m->name);
        write(fd, buffer, n);
    }
    return 0;
}

DEFINE_MODULE(module);
