#include <stdio.h>
#include <errno.h>
#include <string.h>

#include <bcm2835.h>

#include "config.h"
#include "module.h"

#define BUF_SIZE    1024

static char *get_cmd(char *buf, int bufsize, int fd)
{
#if RASPD_USE_CATNET
	return fgets(buf, bufsize, (FILE *)fd);
#else
	printf("recv\n");
	if ((recv(fd, buf, bufsize, 0)) < 0)
        	return NULL;
	char *c = strchr(buf, '\n');
	if (c != NULL)
                *c = '\0';

	return buf;
#endif
}

/*
 * protocol
 *
 *      name:xxx yyy zzz; name2:xxxfff
 *      name: yyy=xxx fff
 */

int do_raspd(int fd)
{
    char buffer[BUF_SIZE];
    int err;

#if ! RASPD_PC_DEBUG
    if (!bcm2835_init())
        return 1;
#endif

    /* main loop */
    while (get_cmd(buffer, sizeof(buffer), fd) != NULL) {
        char *str1, *name, *cmd, *saveptr;

        for (str1 = buffer; ; str1 = NULL) {
            name = strtok_r(str1, ";", &saveptr);
            if (name == NULL)
                break;

            /* skip space */
            while (*name == ' ')
                name++;

            if (cmd = strchr(name, ':')) {
                *cmd++ = '\0';
                while (*cmd == ' ')
                    cmd++;
                if (*cmd == '\0')
                    continue;

                err = invoke_event(name, cmd);
                if (err < 0)
                    fprintf(stderr, "error invoke_event(), err = %d\n", err);
            }
        }
    }

#if ! RASPD_PC_DEBUG
    bcm2835_close();
#endif

    return 0;
}
