#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/time.h>

int main(int argc, char *argv[])
{
    char buf[128];
    struct timeval tv;
	unsigned long long start, stop;
    int fd;
    int i;

    if (argc < 2)
        return 1;

    fd = open(argv[1], O_RDWR);
    if (fd < 0) {
        printf("open %s\n", argv[1]);
        return -EIO;
    }

    gettimeofday(&tv, NULL);
	start = tv.tv_sec * 1000000 + tv.tv_usec;
    
    for (i = 0; i < 100; i++) {
        if (read(fd, buf, sizeof(buf)) < 0) {
            perror("read\n");
            return -EIO;
        }
    }

    gettimeofday(&tv, NULL);
	stop = tv.tv_sec * 1000000 + tv.tv_usec;

    printf("total time: %lld, average time %lld\n", stop - start, (stop - start) / 100);
    close(fd);

	return 0;
}
