#include <stdio.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <stdint.h>
#include <unistd.h>
#include <fcntl.h>
#include <time.h>

#define BCM2835_GPIO_BASE   0x20200000
//#define BCM2835_GPIO_BASE   0x7e200000
#define ACT_GPIO            47
#define PWR_GPIO            35

void set_bits(volatile uint32_t *addr, uint32_t v, uint32_t mask)
{
    uint32_t value = *addr;
    value = (value & ~mask) | (v & mask);
    *addr = value;
    *addr = value;
}

void delay(unsigned int millisec)
{
    struct timespec t;
    t.tv_sec = (time_t)(millisec / 1000);
    t.tv_nsec = (long)(millisec % 1000) * 1000000;
    nanosleep(&t, NULL);
}

int main(int argc, char *argv[])
{
    int fd;
    void *mapped;
    volatile uint32_t *gpio_base;
    int shift;
    int c = 0;
    int sleep_time = 100;
    unsigned int count = -1;
    int io = ACT_GPIO;

    if (argc >= 2)
        sleep_time = atoi(argv[1]);
    if (argc >= 3)
        count = atoi(argv[2]);
    if (argc >= 4)
        io = atoi(argv[3]);

    if ((fd = open("/dev/mem", O_RDWR | O_SYNC)) < 0)
        return 1;

    mapped = mmap(NULL, 0x1000, PROT_READ | PROT_WRITE,
                            MAP_SHARED, fd, BCM2835_GPIO_BASE);
    if (mapped == MAP_FAILED) {
        close(fd);
        return 1;
    }

    gpio_base = (volatile uint32_t *)mapped;

    /* function select */
    shift = (io % 10) * 3;
    set_bits(gpio_base + (io / 10), 1 << shift, 0b111 << shift);

    /* blink */
    while (c <= count) {
        shift = io % 32;
        c++;

        if (c & 1) {
            *(gpio_base + 0x1c / 4 + (io / 32)) = (1 << shift);
            *(gpio_base + 0x1c / 4 + (io / 32)) = (1 << shift);
        } else {
            *(gpio_base + 0x28 / 4 + (io / 32)) = (1 << shift);
            *(gpio_base + 0x28 / 4 + (io / 32)) = (1 << shift);
        }

        delay(sleep_time);
    }

    return 0;
}
