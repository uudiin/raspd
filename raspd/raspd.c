#include <stdio.h>
#include <errno.h>
#include <getopt.h>

#include <bcm2835.h>

#include "module.h"

#define BUF_SIZE    1024

/*
 * protocol
 *
 *      name:xxx yyy zzz; name2:xxxfff
 *      name: yyy=xxx fff
 */

int main(int argc, char *argv[])
{
    char buffer[BUF_SIZE];
    int err;

    if (!bcm2835_init())
        return 1;

    /* main loop */
    while (fgets(buffer, sizeof(buffer), stdin) != NULL) {
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

    bcm2835_close();

    return 0;
}
