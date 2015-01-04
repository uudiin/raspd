#ifndef __LUAENV_H__
#define __LUAENV_H__

int luaenv_getconf_int(const char *table, const char *key, int *v);
int luaenv_getconf_str(const char *table, const char *key, const char **v);
void luaenv_pop(int n);
int luaenv_call_va(const char *func, const char *fmt, ...);
int luaenv_run_file(const char *file);
void *luaenv_getdev(const char *name);
int luaenv_init(void);
void luaenv_exit(void);

#endif /* __LUAENV_H__ */
