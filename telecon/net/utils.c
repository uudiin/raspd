#include <stdio.h>
#include <stdarg.h>
#include <unistd.h>

void err_exit(int retval, const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
    exit(retval);
}
