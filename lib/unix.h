#ifndef __LIB_UNIX_H__
#define __LIB_UNIX_H__

int write_loop(int fd, char *buf, size_t size);
int lockfile(int fd);
int daemonize(const char *cmd);
char **cmdline_split(const char *cmdexec);
int netexec(int fd, const char *cmdexec);
int netrun(int fd, const char *cmdexec);

#endif /* __LIB_UNIX_H__ */
