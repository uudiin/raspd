#include <stdio.h>
#include <errno.h>
#include <getopt.h>

#include "config.h"

int main(int argc, char *argv[])
{
#if RASPD_USE_CATNET
	do_raspd(stdin);
#else
	listen_stream(8888);
#endif

	return 0;
}
