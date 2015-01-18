#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>

#include <bcm2835.h>

#include "softpwm.h"

#define PAGE_SIZE   4096
#define PAGE_SHIFT  12

/* kernel mapped address */
#define DMA_BASE    0x20007000
#define DMA_LEN     0x24

/* bus address */
#define PHYS_GPIO       0x7e200000
#define PHYS_GPCLR0     (PHYS_GPIO + 0x28)
#define PHYS_GPSET0     (PHYS_GPIO + 0x1c)

/* reg index */
#define DMA_CS          (0x00 / 4)
#define DMA_CONBLK_AD   (0x04 / 4)
#define DMA_DEBUG       (0x20 / 4)

#define PWM_CTL         (0x00 / 4)
#define PWM_DMAC        (0x08 / 4)
#define PWM_RNG1        (0x10 / 4)
#define PWM_FIFO        (0x18 / 4)

#define PWMCLK_CNTL     40
#define PWMCLK_DIV      41

/* flags */
#define DMA_NO_WIDE_BURSTS  (1 << 26)
#define DMA_WAIT_RESP       (1 << 3)
#define DMA_D_DREQ          (1 << 6)
#define DMA_PER_MAP(x)      ((x) << 16)
#define DMA_END             (1 << 1)
#define DMA_RESET           (1 << 31)
#define DMA_INT             (1 << 2)

#define PWMCTL_MODE1        (1 << 1)
#define PWMCTL_PWEN1        (1 << 0)
#define PWMCTL_CLRF         (1 << 6)
#define PWMCTL_USEF1        (1 << 5)

#define PWMDMAC_ENAB        (1 << 31)
#define PWMDMAC_THRSHLD     ((15 << 8) | (15 << 0))

/* defined by bcm2835, 8 words, 256 bits */
struct control_blk {
    unsigned long info;
    unsigned long src;
    unsigned long dst;
    unsigned long length;
    unsigned long stride;
    unsigned long next;
    unsigned long __pad[2];
};

struct page_map {
    unsigned char *virt_addr;
    unsigned long phys_addr;
};

volatile static unsigned long *ioreg_clk;
volatile static unsigned long *ioreg_pwm;
volatile static unsigned long *ioreg_dma = MAP_FAILED;

static unsigned char *virtbase = MAP_FAILED;
static struct control_blk *cb;
static unsigned long *sample;
static struct page_map *pagemaps;

static int invert_mode;
static int cycle_time_us;  /* us */
static int sample_time_us; /* us */

static int nr_samples;
//static int nr_channels;
static int nr_pages;

#define MAX_CHANNEl     32

static unsigned long channel_all_mask;
static unsigned long channel_mask;
static int channel_data[MAX_CHANNEl];   /* pin1 - pin32 */

static unsigned long virt_to_phys(void *virt)
{
    unsigned long offset = (unsigned char *)virt - virtbase;
    return pagemaps[offset >> PAGE_SHIFT].phys_addr + (offset % PAGE_SIZE);
}

static void init_ctrl_data(void)
{
    struct control_blk *cbp;
    unsigned long phys_fifo_addr;
    int i;

    cbp = cb;
    /* XXX  ??? */
    phys_fifo_addr = (BCM2835_GPIO_PWM | 0x7e000000) + 0x18;

    memset(sample, 0, nr_samples * sizeof(unsigned long));

    for (i = 0; i < nr_samples; i++)
        sample[i] = channel_all_mask;

	/*
     * Initialize all the DMA commands. They come in pairs.
	 *  - 1st command copies a value from the sample memory to a destination 
	 *    address which can be either the gpclr0 register or the gpset0 register
	 *  - 2nd command waits for a trigger from an external source (PWM or PCM)
	 */
    for (i = 0; i < nr_samples; i++) {
        /* first DMA command */
        cbp->info = DMA_NO_WIDE_BURSTS | DMA_WAIT_RESP;
        cbp->src = virt_to_phys(sample + i);
        if (invert_mode)
            cbp->dst = PHYS_GPSET0;
        else
            cbp->dst = PHYS_GPCLR0;
        cbp->length = sizeof(unsigned long);
        cbp->stride = 0;
        cbp->next = virt_to_phys(cbp + 1);
        cbp++;

        /* second DMA command */
        cbp->info = DMA_NO_WIDE_BURSTS | DMA_WAIT_RESP
                            | DMA_D_DREQ | DMA_PER_MAP(5/*PWM*/);
        cbp->src = virt_to_phys(sample);    /* XXX  ??? */
        cbp->dst = phys_fifo_addr;
        cbp->length = sizeof(unsigned long);
        cbp->stride = 0;
        cbp->next = virt_to_phys(cbp + 1);
        cbp++;
    }
    cbp--;
    cbp->next = virt_to_phys(cb); /* do loop */
}

#define reg_write(r, v) bcm2835_peri_write((volatile uint32_t *)(r), (v))

static void init_hardware(void)
{
    /* initialize PWM */
    reg_write(ioreg_pwm + PWM_CTL, 0);
    reg_write(ioreg_clk + PWMCLK_CNTL, 0x5a000006);   /* src = PLLD (500MHz) */
    reg_write(ioreg_clk + PWMCLK_DIV, 0x5a000000 | (50 << 12));  /* 10MHz */
    reg_write(ioreg_clk + PWMCLK_CNTL, 0x5a000016);   /* src = PLLD, enable */
    reg_write(ioreg_pwm + PWM_RNG1, sample_time_us * 10);  /* XXX  ??? */
    reg_write(ioreg_pwm + PWM_DMAC, PWMDMAC_ENAB | PWMDMAC_THRSHLD);
    reg_write(ioreg_pwm + PWM_CTL, PWMCTL_CLRF);
    reg_write(ioreg_pwm + PWM_CTL, PWMCTL_USEF1 | PWMCTL_PWEN1);

    /* initialize DMA */
    reg_write(ioreg_dma + DMA_CS, DMA_RESET);
    reg_write(ioreg_dma + DMA_CS, DMA_INT | DMA_END);
    reg_write(ioreg_dma + DMA_CONBLK_AD, virt_to_phys(cb));
    reg_write(ioreg_dma + DMA_DEBUG, 7);
    reg_write(ioreg_dma + DMA_CS, 0x10880001); /* go */
}

/*
 * What we need to do here is:
 *   First DMA command turns on the pins that are >0
 *   All the other packets turn off the pins that are not used
 *
 * For the cbp packets (The DMA control packet)
 *  -> cbp[0]->dst = gpset0: set   the pwms that are active
 *  -> cbp[*]->dst = gpclr0: clear when the sample has a value
 *
 * For the samples (The value that is written by the DMA command to cbp[n]->dst)
 *  -> dp[0] = mask of the pwms that are active
 *  -> dp[n] = mask of the pwm to stop at time n
 *
 * We dont really need to reset the cb->dst each time
 * but I believe it helps a lot in code readability
 * in case someone wants to generate more complex signals.
 */
static void update_pwm(void)
{
    unsigned long mask = channel_mask;
    int i, j;

	/*
     * First we turn on the channels that need to be on
     * Take the first DMA Packet and set it's target to start pulse
     */
    if (invert_mode)
        cb[0].dst = PHYS_GPCLR0;
    else
        cb[0].dst = PHYS_GPSET0;

	/* And give that to the DMA controller to write */
    sample[0] = mask;

    /* TODO  optimize this, 64000 loops */
	/* Now we go through all the samples and turn the pins off when needed */
    for (j = 1; j < nr_samples; j++) {
        /* TODO  omit it */
        if (invert_mode)
            cb[j * 2].dst = PHYS_GPSET0;
        else
            cb[j * 2].dst = PHYS_GPCLR0;

        mask = 0;
        for (i = 0; i < MAX_CHANNEl; i++) {
            if (j > channel_data[i])
                mask |= (1 << i);
        }

        sample[j] = mask;
    }
    /*
     * TODO  use this, update_pwm(pin, data)

    for (i = data; i < nr_samples; i++)
        sample[i] |= (1 << pin);
    */
}

/*
 * data:
 *   < 0 : delete pwm
 *   = 0 : set 0 & clear enable mask
 *   > 0 : set data
 */
int softpwm_set_data(int pin, int data)
{
    if (pin >= MAX_CHANNEl)
        return -EINVAL;

    if (data > nr_samples)
        data = nr_samples;

    if (data < 0) {
        channel_all_mask &= ~(1 << pin);
        channel_mask &= ~(1 << pin);
        channel_data[pin] = 0;
    } else {
        channel_all_mask |= (1 << pin);
        channel_data[pin] = data;
        if (data > 0)
            channel_mask |= (1 << pin);
        else if (data == 0)
            channel_mask &= ~(1 << pin);
    }

    bcm2835_gpio_fsel(pin, BCM2835_GPIO_FSEL_OUTP);
    bcm2835_gpio_write(pin, invert_mode ? HIGH : LOW);

    update_pwm();
    return 0;
}

/*
 * /proc/pid/pagemap.  This file lets a userspace process find out which
 * physical frame each virtual page is mapped to.  It contains one 64-bit
 * value for each virtual page, containing the following data :
 *
 *  * Bits 0-54  page frame number (PFN) if present
 *  * Bits 0-4   swap type if swapped
 *  * Bits 5-54  swap offset if swapped
 *  * Bit  55    pte is soft-dirty
 *  * Bits 56-60 zero
 *  * Bit  61    page is file-page or shared-anon
 *  * Bit  62    page swapped
 *  * Bit  63    page present
 */
static int make_pagemap(void)
{
    char pagemap_file[128];
    pid_t pid;
    int fd, memfd;
    unsigned int offset;
    int i;
    int err = 0;

    pagemaps = malloc(nr_pages * sizeof(struct page_map));
    if (pagemaps == NULL)
        return -ENOMEM;
    if ((memfd = open("/dev/mem", O_RDWR)) < 0)
        return -EIO;

    pid = getpid();
    snprintf(pagemap_file, sizeof(pagemap_file), "/proc/%d/pagemap", pid);
    err = -EPERM;
    if ((fd = open(pagemap_file, O_RDONLY)) < 0)
        goto fail_open;

    /* (virt >> 12) * 8 */
    offset = (unsigned int)virtbase >> 9;
    err = -ERANGE;
    if (lseek(fd, offset, SEEK_SET) != offset)
        goto ret;

    for (i = 0; i < nr_pages; i++) {
        unsigned long long pfn;
        pagemaps[i].virt_addr = virtbase + i * PAGE_SIZE;
        /* following line forces page to be allocated */
        pagemaps[i].virt_addr[0] = 0;

        err = -EFAULT;
        if (read(fd, &pfn, sizeof(pfn)) != sizeof(pfn))
            goto ret;
        err = -ESRCH;
        if (((pfn >> 55) & 0x1bf) != 0x10c)
            goto ret;

        pagemaps[i].phys_addr = (unsigned long)pfn << PAGE_SHIFT | 0x40000000;
    }

    err = 0;

ret:
    close(fd);
fail_open:
    close(memfd);
    return err;
}

static void *map_peripheral(unsigned long base, size_t len)
{
    int fd;
    void *virt_addr;
    if ((fd = open("/dev/mem", O_RDWR | O_SYNC)) < 0)
        return MAP_FAILED;
    virt_addr = mmap(NULL, len, PROT_READ | PROT_WRITE, MAP_SHARED, fd, base);
    close(fd);
    return virt_addr;
}

void softpwm_stop(void)
{
    int i;
    if (ioreg_dma == MAP_FAILED || virtbase == MAP_FAILED)
        return;

    for (i = 0; i < MAX_CHANNEl; i++)
        channel_data[i] = 0;
    update_pwm();
    reg_write(ioreg_dma + DMA_CS, DMA_RESET);
}

/*#endef reg_write*/

void softpwm_exit(void)
{
    softpwm_stop();
    if (ioreg_dma != MAP_FAILED)
        munmap((void *)ioreg_dma, DMA_LEN);
    if (virtbase != MAP_FAILED)
        munmap(virtbase, nr_pages * PAGE_SIZE);
    if (pagemaps)
        free(pagemaps);
}

int softpwm_init(int cycle_time, int step_time, int invert)
{
    int size, i;
    int err;

    /*
     * 10 ms (100Hz), step = 5us
     * range (0, 2000)
     * (1ms, 2ms) : (200, 400)
     */ 
    cycle_time_us = cycle_time ?: 10000;
    sample_time_us = step_time ?: 5;
    invert_mode = invert;

    nr_samples = cycle_time_us / sample_time_us;

    assert(sizeof(struct control_blk) == 32);
    size = nr_samples * 2 * sizeof(struct control_blk)
            + nr_samples * sizeof(unsigned long);
    nr_pages = (size + PAGE_SIZE - 1) >> PAGE_SHIFT;

    /* get io reg mapped */
    err = -ENOMEM;
    ioreg_dma = map_peripheral(DMA_BASE, DMA_LEN);
    ioreg_clk = (volatile unsigned long *)bcm2835_regbase(BCM2835_REGBASE_CLK);
    ioreg_pwm = (volatile unsigned long *)bcm2835_regbase(BCM2835_REGBASE_PWM);
    if (ioreg_dma == MAP_FAILED
            || ioreg_clk == MAP_FAILED || ioreg_pwm == MAP_FAILED)
        goto fail;

    /* alloc mem */
    virtbase = mmap(NULL, nr_pages * PAGE_SIZE, PROT_READ | PROT_WRITE,
            MAP_SHARED | MAP_ANONYMOUS | MAP_NORESERVE | MAP_LOCKED, -1, 0);
    err = -ENOMEM;
    if (virtbase == MAP_FAILED)
        goto fail;
    err = -EFAULT;
    if ((unsigned long)virtbase & (PAGE_SIZE - 1))
        goto fail;

    cb = (struct control_blk *)virtbase;
    /*sample = virtbase + nr_samples * 2 * sizeof(struct control_blk);*/
    sample = (unsigned long *)(cb + nr_samples * 2);

    if ((err = make_pagemap()) < 0)
        goto fail;

    for (i = 0; i < MAX_CHANNEl; i++)
        channel_data[i] = 0;

    init_ctrl_data();
    init_hardware();

    return 0;

fail:
    softpwm_exit();
    return err;
}
