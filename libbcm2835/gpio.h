#ifndef __GPIO_H__
#define __GPIO_H__

/* this means pin HIGH, 3.3 volts on a pin */
#define HIGH 1
#define LOW  0

/* defined in device tree */
#define BCM2835_PERI_BASE   0x20000000
#define BCM2835_GPIO_BASE   (BCM2835_PERI_BASE + 0x200000)

/*
 * the BCM2835 has 54 GPIO pins (BCM2835 data sheet, page 90 onwards)
 * GPIO register offsets from BCM2835_GPIO_BASE in bytes
 */
#define GPIO_FSEL0       0x0000 /* GPIO Function Select */
#define GPIO_FSEL1       0x0004
#define GPIO_FSEL2       0x0008
#define GPIO_FSEL3       0x000c
#define GPIO_FSEL4       0x0010
#define GPIO_FSEL5       0x0014
#define GPIO_SET0        0x001c /* GPIO Pin Output Set */
#define GPIO_SET1        0x0020
#define GPIO_CLR0        0x0028 /* GPIO Pin Output Clear */
#define GPIO_CLR1        0x002c
#define GPIO_LEV0        0x0034 /* GPIO Pin Level */
#define GPIO_LEV1        0x0038
#define GPIO_EDS0        0x0040 /* GPIO Pin Event Detect Status */
#define GPIO_EDS1        0x0044
#define GPIO_REN0        0x004c /* GPIO Pin Rising Edge Detect Enable */
#define GPIO_REN1        0x0050
#define GPIO_FEN0        0x0058 /* GPIO Pin Falling Edge Detect Enable */
#define GPIO_FEN1        0x005c
#define GPIO_HEN0        0x0064 /* GPIO Pin High Detect Enable */
#define GPIO_HEN1        0x0068
#define GPIO_LEN0        0x0070 /* GPIO Pin Low Detect Enable */
#define GPIO_LEN1        0x0074
#define GPIO_AREN0       0x007c /* GPIO Pin Async. Rising Edge Detect */
#define GPIO_AREN1       0x0080
#define GPIO_AFEN0       0x0088 /* GPIO Pin Async. Falling Edge Detect */
#define GPIO_AFEN1       0x008c
#define GPIO_PUD         0x0094 /* GPIO Pin Pull-up/down Enable */
#define GPIO_PUDCLK0     0x0098 /* GPIO Pin Pull-up/down Enable Clock */
#define GPIO_PUDCLK1     0x009c

/* function select */
enum gpio_mode {
    GPIO_FSEL_INPT = 0b000,   /* Input */
    GPIO_FSEL_OUTP = 0b001,   /* Output */
    GPIO_FSEL_ALT0 = 0b100,   /* Alternate function 0 */
    GPIO_FSEL_ALT1 = 0b101,   /* Alternate function 1 */
    GPIO_FSEL_ALT2 = 0b110,   /* Alternate function 2 */
    GPIO_FSEL_ALT3 = 0b111,   /* Alternate function 3 */
    GPIO_FSEL_ALT4 = 0b011,   /* Alternate function 4 */
    GPIO_FSEL_ALT5 = 0b010,   /* Alternate function 5 */
    GPIO_FSEL_MASK = 0b111    /* Function select bits mask */
};

enum gpio_pud_control {
    GPIO_PUD_OFF  = 0b00, /* off */
    GPIO_PUD_DOWN = 0b01, /* Enable Pull Down control */
    GPIO_PUD_UP   = 0b01, /* Enable Pull Up control */
};

/* only for Raspberry Pi B+ */
enum rpi_gpio_pin {
    RPI_GPIO_J8_03 =  2,
    RPI_GPIO_J8_05 =  3,
    RPI_GPIO_J8_07 =  4,
    RPI_GPIO_J8_08 = 14,  /* defaults to alt function 0 UART0_TXD */
    RPI_GPIO_J8_10 = 15,  /* defaults to alt function 0 UART0_RXD */
    RPI_GPIO_J8_11 = 17,
    RPI_GPIO_J8_12 = 18,  /* can be PWM channel 0 in ALT FUN 5 */
    RPI_GPIO_J8_13 = 27,
    RPI_GPIO_J8_15 = 22,
    RPI_GPIO_J8_16 = 23,
    RPI_GPIO_J8_18 = 24,
    RPI_GPIO_J8_19 = 10,  /* MOSI when SPI0 in use */
    RPI_GPIO_J8_21 =  9,  /* MISO when SPI0 in use */
    RPI_GPIO_J8_22 = 25,
    RPI_GPIO_J8_23 = 11,  /* CLK when SPI0 in use */
    RPI_GPIO_J8_24 =  8,  /* CE0 when SPI0 in use */
    RPI_GPIO_J8_26 =  7,  /* CE1 when SPI0 in use */
    RPI_GPIO_J8_29 =  5,
    RPI_GPIO_J8_31 =  6,
    RPI_GPIO_J8_32 =  12,
    RPI_GPIO_J8_33 =  13,
    RPI_GPIO_J8_35 =  19,
    RPI_GPIO_J8_36 =  16,
    RPI_GPIO_J8_37 =  26,
    RPI_GPIO_J8_38 =  20,
    RPI_GPIO_J8_40 =  21,
};


#endif /* __GPIO_H__ */
