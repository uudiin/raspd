#include <stdlib.h>
#include <unistd.h>

#include "xmalloc.h"

void *xmalloc(size_t size)
{
    void *p = malloc(size);
    if (p == NULL)
        _exit(1);
    return p;
}
