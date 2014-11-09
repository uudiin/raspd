#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <getopt.h>

#include <bcm2835.h>

#include "module.h"

#define BUF_SIZE    1024

/*
 * protocol
 *
 *      name xxx yyy zzz; name2 xxxfff
 *      name yyy=xxx fff
 */

int main(int argc, char *argv[])
{
    char buffer[BUF_SIZE];
    int err;

    if (!bcm2835_init())
        return 1;

    /* main loop */
    while (fgets(buffer, sizeof(buffer), stdin) != NULL) {
        char *str, *cmdexec, *saveptr;

        for (str = buffer; cmdexec = strtok_r(str, ";", &saveptr); str = NULL) {

            err = module_cmdexec(cmdexec);
            if (err != 0)
                fprintf(stderr, "module_cmdexec(%s), err = %d\n", cmdexec, err);
        }
    }

    bcm2835_close();

    return 0;
}
