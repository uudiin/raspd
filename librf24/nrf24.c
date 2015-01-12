/*
   Copyright (C) 2011 J. Coliz <maniacbug@ymail.com>

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License
   version 2 as published by the Free Software Foundation.



   03/17/2013 : Charles-Henri Hallard (http://hallard.me)
   Modified to use with Arduipi board http://hallard.me/arduipi
   Changed to use modified bcm2835 library

   TMRh20 2014: Updated to work with optimized RF24 and RF24 Network Arduino libs.
   */

#include <stdio.h>
#include <string.h>

#include "nrf24.h"
#include "nRF24L01.h"

// GCC a Arduino Missing
#define max(a,b) (a>b?a:b)
#define min(a,b) (a<b?a:b)
#define _BV(x) (1<<(x))
#define pgm_read_word(p) (*(p))
#define pgm_read_byte(p) (*(p))

/**
 * Read a chunk of data in from a register
 *
 * @param reg Which register. Use constants from nRF24L01.h
 * @param buf Where to put the data
 * @param len How many bytes of data to transfer
 * @return Current value of status register
 */
uint8_t read_register_buf(uint8_t reg, uint8_t *buf, uint8_t len);

/**
 * Read single byte from a register
 *
 * @param reg Which register. Use constants from nRF24L01.h
 * @return Current value of register @p reg
 */
uint8_t read_register(uint8_t reg);

/**
 * Write a chunk of data to a register
 *
 * @param reg Which register. Use constants from nRF24L01.h
 * @param buf Where to get the data
 * @param len How many bytes of data to transfer
 * @return Current value of status register
 */
uint8_t write_register_buf(uint8_t reg, const uint8_t *buf, uint8_t len);

/**
 * Write a single byte to a register
 *
 * @param reg Which register. Use constants from nRF24L01.h
 * @param value The new value to write
 * @return Current value of status register
 */
uint8_t write_register(uint8_t reg, uint8_t value);

/**
 * Write the transmit payload
 *
 * The size of data written is the fixed payload size, see getPayloadSize()
 *
 * @param buf Where to get the data
 * @param len Number of bytes to be sent
 * @return Current value of status register
 */
uint8_t write_payload(const void *buf, uint8_t len, const uint8_t writeType);

/**
 * Read the receive payload
 *
 * The size of data read is the fixed payload size, see getPayloadSize()
 *
 * @param buf Where to put the data
 * @param len Maximum number of bytes to read
 * @return Current value of status register
 */
uint8_t read_payload(void *buf, uint8_t len);

/**
 * Empty the receive buffer
 *
 * @return Current value of status register
 */
uint8_t flush_rx(void);

/**
 * Retrieve the current status of the chip
 *
 * @return Current value of status register
 */
uint8_t get_status(void);

/**
 * Decode and print the given status to stdout
 *
 * @param status Status value to print
 *
 * @warning Does nothing if stdout is not defined.  See fdevopen in stdio.h
 */
void print_status(uint8_t status);

/**
 * Decode and print the given 'observe_tx' value to stdout
 *
 * @param value The observe_tx value to print
 *
 * @warning Does nothing if stdout is not defined.  See fdevopen in stdio.h
 */
void print_observe_tx(uint8_t value);

/**
 * Print the name and value of an 8-bit register to stdout
 *
 * Optionally it can print some quantity of successive
 * registers on the same line.  This is useful for printing a group
 * of related registers on one line.
 *
 * @param name Name of the register
 * @param reg Which register. Use constants from nRF24L01.h
 * @param qty How many successive registers to print
 */
void print_byte_register(const char *name, uint8_t reg, uint8_t qty);

/**
 * Print the name and value of a 40-bit address register to stdout
 *
 * Optionally it can print some quantity of successive
 * registers on the same line.  This is useful for printing a group
 * of related registers on one line.
 *
 * @param name Name of the register
 * @param reg Which register. Use constants from nRF24L01.h
 * @param qty How many successive registers to print
 */
void print_address_register(const char *name, uint8_t reg, uint8_t qty);

/**
 * Turn on or off the special features of the chip
 *
 * The chip has certain 'features' which are only available when the 'features'
 * are enabled.  See the datasheet for details.
 */
void toggle_features(void);

static uint8_t ce_pin; /**< "Chip Enable" pin, activates the RX or TX role */
static uint8_t csn_pin; /**< SPI Chip select */
static uint16_t spi_speed; /**< SPI Bus Speed */
static bool p_variant; /* False for RF24L01 and true for RF24L01P */
static uint8_t payload_size; /**< Fixed size of payloads */

static bool dynamic_payloads_enabled; /**< Whether dynamic payloads are enabled. */
static uint8_t pipe0_reading_address[5]; /**< Last address set on pipe 0 for reading. */
static uint8_t addr_width;

static uint8_t debug; /* Debug flag */
static uint8_t spi_rxbuff[32 + 1]; //SPI receive buffer (payload max 32 bytes)
static uint8_t spi_txbuff[32 + 1]; //SPI transmit buffer (payload max 32 bytes + 1 byte for the command)

/****************************************************************************/
uint8_t read_register_buf(uint8_t reg, uint8_t *buf, uint8_t len)
{
    uint8_t status;
    uint8_t *prx = spi_rxbuff;
    uint8_t *ptx = spi_txbuff;
    uint8_t size = len + 1; // Add register value to transmit buffer

    *ptx++ = (R_REGISTER | (REGISTER_MASK & reg));
    while (len--) {
        *ptx++ = NOP; // Dummy operation, just for reading
    }
    bcm2835_spi_transfernb((char *)spi_txbuff, (char *)spi_rxbuff, size);

    status = *prx++; // status is 1st byte of receive buffer

    // decrement before to skip status byte
    while (--size) {
        *buf++ = *prx++;
    }

    return status;
} 

/****************************************************************************/
uint8_t read_register(uint8_t reg)
{
    uint8_t result;

    uint8_t *prx = spi_rxbuff;
    uint8_t *ptx = spi_txbuff;

    *ptx++ = (R_REGISTER | (REGISTER_MASK & reg));
    *ptx++ = NOP; // Dummy operation, just for reading

    bcm2835_spi_transfernb((char *)spi_txbuff, (char *)spi_rxbuff, 2);

    result = *++prx;   // result is 2nd byte of receive buffer

    return result;
}

/****************************************************************************/

uint8_t write_register(uint8_t reg, uint8_t value)
{
    uint8_t status;

    uint8_t *prx = spi_rxbuff;
    uint8_t *ptx = spi_txbuff;

    *ptx++ = (W_REGISTER | (REGISTER_MASK & reg));
    *ptx = value;

    bcm2835_spi_transfernb((char *)spi_txbuff, (char *)spi_rxbuff, 2);  

    status = *prx++; // status is 1st byte of receive buffer

    if (debug)
        printf("write_register(%02x,%02x)\r\n", reg, value);

    return status;
}

/****************************************************************************/

uint8_t write_register_buf(uint8_t reg, const uint8_t *buf, uint8_t len)
{
    uint8_t status;
    uint8_t * prx = spi_rxbuff;
    uint8_t * ptx = spi_txbuff;
    uint8_t size = len + 1; // Add register value to transmit buffer

    *ptx++ = (W_REGISTER | (REGISTER_MASK & reg));
    while (len--)
        *ptx++ = *buf++;

    bcm2835_spi_transfernb((char *)spi_txbuff, (char *)spi_rxbuff, size);

    status = *prx; // status is 1st byte of receive buffer

    return status;
}


/****************************************************************************/

uint8_t write_payload(const void *buf, uint8_t len, const uint8_t writeType)
{
    uint8_t status;
    uint8_t *prx = spi_rxbuff;
    uint8_t *ptx = spi_txbuff;
    uint8_t size;

    const uint8_t *current = (const uint8_t *)(buf);

    uint8_t data_len = min(len, payload_size);
    uint8_t blank_len = dynamic_payloads_enabled ? 0 : payload_size - data_len;

    size = data_len + blank_len + 1; // Add register value to transmit buffer

    if (debug)
        printf("[Writing %u bytes %u blanks]", data_len, blank_len);

    *ptx++ =  W_TX_PAYLOAD;
    while (data_len--)
        *ptx++ =  *current++;
    while (blank_len--)
        *ptx++ =  0;

    bcm2835_spi_transfernb((char *)spi_txbuff, (char *)spi_rxbuff, size);

    status = *prx; // status is 1st byte of receive buffer

    return status;
}

/****************************************************************************/

uint8_t read_payload(void *buf, uint8_t len)
{
    uint8_t status;
    uint8_t *prx = spi_rxbuff;
    uint8_t *ptx = spi_txbuff;
    uint8_t size;

    uint8_t *current = (uint8_t *)(buf);

    uint8_t data_len = min(len, payload_size);
    uint8_t blank_len = dynamic_payloads_enabled ? 0 : payload_size - data_len;

    size = data_len + blank_len + 1; // Add register value to transmit buffer

    if (debug)
        printf("[Reading %u bytes %u blanks]", data_len, blank_len);

    *ptx++ =  R_RX_PAYLOAD;
    while (size--)
        *ptx++ = NOP;

    // Size has been lost during while, re affect
    size = data_len + blank_len + 1; // Add register value to transmit buffer

    bcm2835_spi_transfernb((char *)spi_txbuff, (char *)spi_rxbuff, size);

    // 1st byte is status
    status = *prx++;

    // Decrement before to skip 1st status byte
    while (--size)
        *current++ = *prx++;

    return status;
}

/****************************************************************************/

uint8_t flush_rx(void)
{
    uint8_t status;

    status = bcm2835_spi_transfer(FLUSH_RX);

    return status;
}

/****************************************************************************/

uint8_t rf24_flush_tx(void)
{
    uint8_t status;

    status = bcm2835_spi_transfer(FLUSH_TX);

    return status;
}

/****************************************************************************/

uint8_t get_status(void)
{
    return bcm2835_spi_transfer(NOP);
}

/****************************************************************************/

void print_status(uint8_t status)
{
    printf("STATUS\t\t = 0x%02x RX_DR=%x TX_DS=%x MAX_RT=%x RX_P_NO=%x TX_FULL=%x\r\n",
            status,
            (status & _BV(RX_DR)) ? 1 : 0,
            (status & _BV(TX_DS)) ? 1 : 0,
            (status & _BV(MAX_RT)) ? 1 : 0,
            ((status >> RX_P_NO) & 0b111),
            (status & _BV(TX_FULL)) ? 1 : 0
         );
}

/****************************************************************************/

void print_observe_tx(uint8_t value)
{
    printf("OBSERVE_TX=%02x: POLS_CNT=%x ARC_CNT=%x\r\n",
            value,
            (value >> PLOS_CNT) & 0b1111,
            (value >> ARC_CNT) & 0b1111
         );
}

/****************************************************************************/

void print_byte_register(const char* name, uint8_t reg, uint8_t qty)
{
    char extra_tab = strlen(name) < 8 ? '\t' : 0;
    printf("%s\t%c =", name, extra_tab);
    while (qty--)
        printf(" 0x%02x", read_register(reg++));
    printf("\n");
}

/****************************************************************************/

void print_address_register(const char *name, uint8_t reg, uint8_t qty)
{
    char extra_tab = strlen(name) < 8 ? '\t' : 0;
    printf("%s\t%c =", name, extra_tab);

    while (qty--) {
        uint8_t buffer[addr_width];
        read_register_buf(reg++, buffer, sizeof buffer);

        printf(" 0x");
        uint8_t* bufptr = buffer + sizeof buffer;
        while (--bufptr >= buffer)
            printf("%02x", *bufptr);
    }

    printf("\r\n");
}


/****************************************************************************/

void RF24(uint8_t _cepin, uint8_t _cspin, uint32_t _spi_speed)
{	
    ce_pin = _cepin;
    csn_pin = _cspin;
    spi_speed = _spi_speed;
    p_variant = false;
    payload_size = 32;
    dynamic_payloads_enabled = false;
    addr_width = 5;//,pipe0_reading_address(0) 
}

/****************************************************************************/

void rf24_setChannel(uint8_t channel)
{
    const uint8_t max_channel = 127;
    write_register(RF_CH, min(channel, max_channel));
}

/****************************************************************************/

void rf24_setPayloadSize(uint8_t size)
{
    const uint8_t max_payload_size = 32;
    payload_size = min(size, max_payload_size);
}

/****************************************************************************/

uint8_t rf24_getPayloadSize(void)
{
    return payload_size;
}

/****************************************************************************/

static const char rf24_datarate_e_str_0[] = "1MBPS";
static const char rf24_datarate_e_str_1[] = "2MBPS";
static const char rf24_datarate_e_str_2[] = "250KBPS";
static const char * const rf24_datarate_e_str_P[] = {
    rf24_datarate_e_str_0,
    rf24_datarate_e_str_1,
    rf24_datarate_e_str_2,
};
static const char rf24_model_e_str_0[] = "nRF24L01";
static const char rf24_model_e_str_1[] = "nRF24L01+";
static const char * const rf24_model_e_str_P[] = {
    rf24_model_e_str_0,
    rf24_model_e_str_1,
};
static const char rf24_crclength_e_str_0[] = "Disabled";
static const char rf24_crclength_e_str_1[] = "8 bits";
static const char rf24_crclength_e_str_2[] = "16 bits";
static const char * const rf24_crclength_e_str_P[]  = {
    rf24_crclength_e_str_0,
    rf24_crclength_e_str_1,
    rf24_crclength_e_str_2,
};
static const char rf24_pa_dbm_e_str_0[] = "PA_MIN";
static const char rf24_pa_dbm_e_str_1[] = "PA_LOW";
static const char rf24_pa_dbm_e_str_2[] = "PA_HIGH";
static const char rf24_pa_dbm_e_str_3[] = "PA_MAX";
static const char * const rf24_pa_dbm_e_str_P[] = {
    rf24_pa_dbm_e_str_0,
    rf24_pa_dbm_e_str_1,
    rf24_pa_dbm_e_str_2,
    rf24_pa_dbm_e_str_3,
};

static const char rf24_csn_e_str_0[] = "CE0 (PI Hardware Driven)";
static const char rf24_csn_e_str_1[] = "CE1 (PI Hardware Driven)";
static const char rf24_csn_e_str_2[] = "CE2 (PI Hardware Driven)";
static const char rf24_csn_e_str_3[] = "Custom GPIO Software Driven";
static const char * const rf24_csn_e_str_P[] = {
    rf24_csn_e_str_0,
    rf24_csn_e_str_1,
    rf24_csn_e_str_2,
    rf24_csn_e_str_3,
};

// Display NRF24L01 details
void rf24_printDetails(void)
{
    printf("================ SPI Configuration ================\n");

    if (csn_pin < BCM2835_SPI_CS_NONE) {
        printf("CSN Pin  \t = %s\n", rf24_csn_e_str_P[csn_pin]);
    } else {
        printf("CSN Pin  \t = Custom GPIO%d%s\n", csn_pin,
                csn_pin == RPI_V2_GPIO_P1_26 ? " (CE1) Software Driven" : "");
    }

    printf("CE Pin  \t = Custom GPIO%d\n", ce_pin);

    // SPI Bus Speed
    printf("Clock Speed\t = ");
    switch (spi_speed) {
        case BCM2835_SPI_SPEED_64MHZ : printf("64 Mhz");	break;
        case BCM2835_SPI_SPEED_32MHZ : printf("32 Mhz");	break;
        case BCM2835_SPI_SPEED_16MHZ : printf("16 Mhz");	break;
        case BCM2835_SPI_SPEED_8MHZ  : printf("8 Mhz");	break;
        case BCM2835_SPI_SPEED_4MHZ  : printf("4 Mhz");	break;
        case BCM2835_SPI_SPEED_2MHZ  : printf("2 Mhz");	break;
        case BCM2835_SPI_SPEED_1MHZ  : printf("1 Mhz");	break;
        case BCM2835_SPI_SPEED_512KHZ: printf("512 KHz");	break;
        case BCM2835_SPI_SPEED_256KHZ: printf("256 KHz");	break;
        case BCM2835_SPI_SPEED_128KHZ: printf("128 KHz");	break;
        case BCM2835_SPI_SPEED_64KHZ : printf("64 KHz");	break;
        case BCM2835_SPI_SPEED_32KHZ : printf("32 KHz");	break;
        case BCM2835_SPI_SPEED_16KHZ : printf("16 KHz");	break;
        case BCM2835_SPI_SPEED_8KHZ  : printf("8 KHz");	break;
        default : printf("Probably Bad !!!");	break;
    }
    printf("\n");

    printf("================ NRF Configuration ================\n");

    print_status(get_status());

    print_address_register("RX_ADDR_P0-1", RX_ADDR_P0, 2);
    print_byte_register("RX_ADDR_P2-5", RX_ADDR_P2, 4);
    print_address_register("TX_ADDR", TX_ADDR, 1);

    print_byte_register("RX_PW_P0-6", RX_PW_P0, 6);
    print_byte_register("EN_AA", EN_AA, 1);
    print_byte_register("EN_RXADDR", EN_RXADDR, 1);
    print_byte_register("RF_CH", RF_CH, 1);
    print_byte_register("RF_SETUP", RF_SETUP, 1);
    print_byte_register("DYNPD/FEATURE", DYNPD, 2);

    printf("Data Rate\t = %s\r\n", rf24_datarate_e_str_P[rf24_getDataRate()]);
    printf("Model\t\t = %s\r\n", rf24_model_e_str_P[rf24_isPVariant()]);
    printf("CRC Length\t = %s\r\n", rf24_crclength_e_str_P[rf24_getCRCLength()]);
    printf("PA Power\t = %s\r\n", rf24_pa_dbm_e_str_P[rf24_getPALevel()]);
}

/****************************************************************************/

bool rf24_begin(void)
{
    // This initialize the SPI bus with
    // csn pin as chip select (custom or not)


    // Init BCM2835 chipset for talking with us
    if (!bcm2835_init()) {
        return false;
    }

    switch(csn_pin) {     //Ensure valid hardware CS pin
        case 0: break;
        case 1: break;
        case 8: csn_pin = 0; break;
        case 7: csn_pin = 1; break;
        default: csn_pin = 0; break;
    }


    bcm2835_spi_begin();

    // used to drive custom I/O to trigger my logic analyser
    // bcm2835_gpio_fsel(GPIO_CTRL_PIN , BCM2835_GPIO_FSEL_OUTP);

    // start the SPI library:
    // Note the NRF24 wants mode 0, MSB first and default to 1 Mbps
    bcm2835_spi_setBitOrder(BCM2835_SPI_BIT_ORDER_MSBFIRST);
    bcm2835_spi_setDataMode(BCM2835_SPI_MODE0);

    // Set SPI bus Speed
    bcm2835_spi_setClockDivider(spi_speed);

    // Choose hardware CSN pin
    bcm2835_spi_chipSelect(csn_pin);


    // Initialise the CE pin of NRF24 (chip enable) after the CSN pin, so that
    // The input mode is not changed if using one of the hardware CE pins
    bcm2835_gpio_fsel(ce_pin, BCM2835_GPIO_FSEL_OUTP);
    bcm2835_gpio_write(ce_pin, LOW);

    // wait 100ms
    delay(100);

    // Set 1500uS (minimum for 32B payload in ESB@250KBPS) timeouts, to make testing a little easier
    // WARNING: If this is ever lowered, either 250KBS mode with AA is broken or maximum packet
    // sizes must never be used. See documentation for a more complete explanation.
    //printf("write_register(%02X, %02X)\n", SETUP_RETR, (0b0100 << ARD) | (0b1111 << ARC));
    rf24_setRetries(5, 15);

    // Determine if this is a p or non-p RF24 module and then
    // reset our data rate back to default value. This works
    // because a non-P variant won't allow the data rate to
    // be set to 250Kbps.
    if (rf24_setDataRate(RF24_250KBPS)) {
        p_variant = true;
    }

    // Then set the data rate to the slowest (and most reliable) speed supported by all
    // hardware.
    rf24_setDataRate(RF24_1MBPS);

    // Initialize CRC and request 2-byte (16bit) CRC
    rf24_setCRCLength(RF24_CRC_16);

    toggle_features();
    write_register(FEATURE, 0);
    write_register(DYNPD, 0);

    // Reset current status
    // Notice reset and flush is the last thing we do
    write_register(STATUS,_BV(RX_DR) | _BV(TX_DS) | _BV(MAX_RT));

    // Set up default configuration.  Callers can always change it later.
    // This channel should be universally safe and not bleed over into adjacent
    // spectrum.
    rf24_setChannel(76);

    // Flush buffers
    flush_rx();
    rf24_flush_tx();

    rf24_powerUp();

    // Enable PTX, do not write CE high so radio will remain in standby I mode (130us max to transition to RX or TX instead of 1500us from powerUp)
    // PTX should use only 22uA of power
    write_register(CONFIG, (read_register(CONFIG)) & ~_BV(PRIM_RX));

    return true;
}

/****************************************************************************/

void rf24_startListening(void)
{
    rf24_powerUp();
    write_register(CONFIG, read_register(CONFIG) | _BV(PRIM_RX));
    write_register(STATUS, _BV(RX_DR) | _BV(TX_DS) | _BV(MAX_RT));

    // Restore the pipe0 adddress, if exists
    if (pipe0_reading_address[0] > 0) {
        write_register_buf(RX_ADDR_P0, pipe0_reading_address, addr_width);	
    }
    // Flush buffers
    //flush_rx();
    rf24_flush_tx();

    // Go!
    bcm2835_gpio_write(ce_pin, HIGH);

    // wait for the radio to come up (130us actually only needed)
    delayMicroseconds(130);
}

/****************************************************************************/

void rf24_stopListening(void)
{
    bcm2835_gpio_write(ce_pin, LOW);
    rf24_flush_tx();
    flush_rx();
    delayMicroseconds(150);
    write_register(CONFIG, (read_register(CONFIG)) & ~_BV(PRIM_RX));  
    delayMicroseconds(150);
}

/****************************************************************************/

void rf24_powerDown(void)
{
    bcm2835_gpio_write(ce_pin, LOW);
    write_register(CONFIG,read_register(CONFIG) & ~_BV(PWR_UP));
}

/****************************************************************************/

void rf24_powerUp(void)
{
    bool up = read_register(CONFIG) & _BV(PWR_UP);

    if (!up) {
        write_register(CONFIG, (read_register(CONFIG) | _BV(PWR_UP)));
        delay(5);
    }
}

/******************************************************************/

/******************************************************************/

bool rf24_write_multi(const void *buf, uint8_t len, const bool multicast)
{
    // Begin the write
    rf24_startFastWrite(buf, len, multicast);


    // If this hangs, it ain't coming back, no sense in timing out
    while (!(get_status()  & (_BV(TX_DS) | _BV(MAX_RT)))) { 
    }
    bcm2835_gpio_write(ce_pin, LOW);
    uint8_t status = write_register(STATUS, _BV(RX_DR) | _BV(TX_DS) | _BV(MAX_RT));


    //Max retries exceeded
    if (status & _BV(MAX_RT)) {
        rf24_flush_tx(); //Only going to be 1 packet int the FIFO at a time using this method, so just flush
        return false;
    }
    //TX OK 1 or 0
    return true;
}

bool rf24_write(const void *buf, uint8_t len)
{
    return rf24_write_multi(buf, len, 0);
}

/****************************************************************************/

void rf24_reUseTX(void)
{
    write_register(STATUS,_BV(MAX_RT));			  //Clear max retry flag
    //spiTrans(REUSE_TX_PL);
    bcm2835_spi_transfer(REUSE_TX_PL);
    bcm2835_gpio_write(ce_pin, LOW);										  //Re-Transfer packet
    bcm2835_gpio_write(ce_pin, HIGH);
}

/****************************************************************************/

bool rf24_writeFast_multi(const void* buf, uint8_t len, const bool multicast)
{
    //Block until the FIFO is NOT full.
    //Keep track of the MAX retries and set auto-retry if seeing failures
    //Return 0 so the user can control the retrys and set a timer or failure counter if required
    //The radio will auto-clear everything in the FIFO as long as CE remains high


    while ((get_status()  & (_BV(TX_FULL)))) {			  //Blocking only if FIFO is full. This will loop and block until TX is successful or fail

        if (get_status() & _BV(MAX_RT)) {
            //reUseTX();										  //Set re-transmit
            write_register(STATUS, _BV(MAX_RT));			  //Clear max retry flag
            return 0;										  //Return 0. The previous payload has been retransmitted
            //From the user perspective, if you get a 0, just keep trying to send the same payload
        }
    }
    //Start Writing
    rf24_startFastWrite(buf, len, multicast);

    return 1;
}

bool rf24_writeFast(const void* buf, uint8_t len)
{
    return rf24_writeFast_multi(buf, len, 0);
}

/****************************************************************************/

//Per the documentation, we want to set PTX Mode when not listening. Then all we do is write data and set CE high
//In this mode, if we can keep the FIFO buffers loaded, packets will transmit immediately (no 130us delay)
//Otherwise we enter Standby-II mode, which is still faster than standby mode
//Also, we remove the need to keep writing the config register over and over and delaying for 150 us each time if sending a stream of data

void rf24_startFastWrite(const void *buf, uint8_t len, const bool multicast)
{ //TMRh20

    //write_payload(buf,len);
    write_payload(buf, len, multicast ? W_TX_PAYLOAD_NO_ACK : W_TX_PAYLOAD);
    bcm2835_gpio_write(ce_pin, HIGH);

}

/****************************************************************************/

void rf24_startWrite(const void *buf, uint8_t len, const bool multicast)
{

    // Send the payload

    write_payload(buf, len,multicast ? W_TX_PAYLOAD_NO_ACK : W_TX_PAYLOAD);
    bcm2835_gpio_write(ce_pin, HIGH);
    delayMicroseconds(10);
    bcm2835_gpio_write(ce_pin, LOW);
}

/****************************************************************************/

bool rf24_txStandBy(void)
{
    while (!(read_register(FIFO_STATUS) & _BV(TX_EMPTY))){
        if (get_status() & _BV(MAX_RT)) {
            write_register(STATUS, _BV(MAX_RT));
            bcm2835_gpio_write(ce_pin, LOW);
            rf24_flush_tx();    //Non blocking, flush the data
            return 0;
        }
    }

    bcm2835_gpio_write(ce_pin, LOW);			   //Set STANDBY-I mode
    return 1;
}

/****************************************************************************/

void rf24_maskIRQ(bool tx, bool fail, bool rx){

    write_register(CONFIG, (read_register(CONFIG)) | fail << MASK_MAX_RT | tx << MASK_TX_DS | rx << MASK_RX_DR );
}

/****************************************************************************/

uint8_t rf24_getDynamicPayloadSize(void)
{
    spi_txbuff[0] = R_RX_PL_WID;
    spi_rxbuff[1] = 0xff;

    bcm2835_spi_transfernb((char *)spi_txbuff, (char *)spi_rxbuff, 2);

    if (spi_rxbuff[1] > 32) { flush_rx(); return 0; }

    return spi_rxbuff[1];
}

/****************************************************************************/

bool rf24_available(void)
{
    return rf24_available_pipe(NULL);
}

/****************************************************************************/

bool rf24_available_pipe(uint8_t* pipe_num)
{
    //Check the FIFO buffer to see if data is waitng to be read
    if (!(read_register(FIFO_STATUS) & _BV(RX_EMPTY))) {

        // If the caller wants the pipe number, include that
        if (pipe_num) {
            uint8_t status = get_status();
            *pipe_num = (status >> RX_P_NO) & 0b111;
        }
        return 1;
    }

    return 0;
}

/****************************************************************************/

void rf24_read(void* buf, uint8_t len)
{
    // Fetch the payload
    read_payload(buf, len);

    //Clear the two possible interrupt flags with one command
    write_register(STATUS, _BV(RX_DR) | _BV(MAX_RT) | _BV(TX_DS));
}

/****************************************************************************/

void rf24_whatHappened(bool *tx_ok, bool *tx_fail, bool *rx_ready)
{
    // Read the status & reset the status in one easy call
    // Or is that such a good idea?
    uint8_t status = write_register(STATUS, _BV(RX_DR) | _BV(TX_DS) | _BV(MAX_RT));

    // Report to the user what happened
    *tx_ok = status & _BV(TX_DS);
    *tx_fail = status & _BV(MAX_RT);
    *rx_ready = status & _BV(RX_DR);
}

/****************************************************************************/

void rf24_openWritingPipe64(uint64_t value)
{
    // Note that AVR 8-bit uC's store this LSB first, and the NRF24L01(+)
    // expects it LSB first too, so we're good.

    write_register_buf(RX_ADDR_P0, (uint8_t*)(&value), addr_width);
    write_register_buf(TX_ADDR, (uint8_t*)(&value), addr_width);


    write_register(RX_PW_P0, payload_size);
}

/****************************************************************************/
void rf24_openWritingPipe(const uint8_t *address)
{
    // Note that AVR 8-bit uC's store this LSB first, and the NRF24L01(+)
    // expects it LSB first too, so we're good.

    write_register_buf(RX_ADDR_P0, address, addr_width);
    write_register_buf(TX_ADDR, address, addr_width);

    //const uint8_t max_payload_size = 32;
    //write_register(RX_PW_P0,min(payload_size,max_payload_size));
    write_register(RX_PW_P0, payload_size);
}

/****************************************************************************/

static const uint8_t child_pipe[] =
{
    RX_ADDR_P0, RX_ADDR_P1, RX_ADDR_P2, RX_ADDR_P3, RX_ADDR_P4, RX_ADDR_P5
};

static const uint8_t child_payload_size[] =
{
    RX_PW_P0, RX_PW_P1, RX_PW_P2, RX_PW_P3, RX_PW_P4, RX_PW_P5
};

static const uint8_t child_pipe_enable[] =
{
    ERX_P0, ERX_P1, ERX_P2, ERX_P3, ERX_P4, ERX_P5
};

void rf24_openReadingPipe64(uint8_t child, uint64_t address)
{
    // If this is pipe 0, cache the address.  This is needed because
    // openWritingPipe() will overwrite the pipe 0 address, so
    // startListening() will have to restore it.
    if (child == 0) {
        memcpy(pipe0_reading_address, &address,addr_width);
    }

    if (child <= 6) {
        // For pipes 2-5, only write the LSB
        if (child < 2)
            write_register_buf(pgm_read_byte(&child_pipe[child]), (const uint8_t*)(&address), addr_width);
        else
            write_register_buf(pgm_read_byte(&child_pipe[child]), (const uint8_t*)(&address), 1);

        write_register(pgm_read_byte(&child_payload_size[child]), payload_size);

        // Note it would be more efficient to set all of the bits for all open
        // pipes at once.  However, I thought it would make the calling code
        // more simple to do it this way.
        write_register(EN_RXADDR, read_register(EN_RXADDR) | _BV(pgm_read_byte(&child_pipe_enable[child])));
    }
}

/****************************************************************************/

void rf24_setAddressWidth(uint8_t a_width)
{

    if (a_width -= 2) {
        write_register(SETUP_AW, a_width%4);
        addr_width = (a_width % 4) + 2;
    }

}

/****************************************************************************/

void rf24_openReadingPipe(uint8_t child, const uint8_t *address)
{
    // If this is pipe 0, cache the address.  This is needed because
    // openWritingPipe() will overwrite the pipe 0 address, so
    // startListening() will have to restore it.
    if (child == 0) {
        memcpy(pipe0_reading_address,address,addr_width);
    }
    if (child <= 6) {
        // For pipes 2-5, only write the LSB
        if (child < 2) {
            write_register_buf(pgm_read_byte(&child_pipe[child]), address, addr_width);
        } else {
            write_register_buf(pgm_read_byte(&child_pipe[child]), address, 1);
        }
        write_register(pgm_read_byte(&child_payload_size[child]), payload_size);

        // Note it would be more efficient to set all of the bits for all open
        // pipes at once.  However, I thought it would make the calling code
        // more simple to do it this way.
        write_register(EN_RXADDR, read_register(EN_RXADDR) | _BV(pgm_read_byte(&child_pipe_enable[child])));

    }
}

/****************************************************************************/

void toggle_features(void)
{
    bcm2835_spi_transfer(ACTIVATE);
    bcm2835_spi_transfer(0x73);
}

/****************************************************************************/

void rf24_enableDynamicPayloads(void)
{

    // So enable them and try again
    toggle_features();
    write_register(FEATURE, read_register(FEATURE) | _BV(EN_DPL));

    if (debug)
        printf("FEATURE=%i\r\n", read_register(FEATURE));

    // Enable dynamic payload on all pipes
    //
    // Not sure the use case of only having dynamic payload on certain
    // pipes, so the library does not support it.
    write_register(DYNPD, read_register(DYNPD) | _BV(DPL_P5) | _BV(DPL_P4) | _BV(DPL_P3) | _BV(DPL_P2) | _BV(DPL_P1) | _BV(DPL_P0));

    dynamic_payloads_enabled = true;
}

/****************************************************************************/

void rf24_enableAckPayload(void)
{
    //
    // enable ack payload and dynamic payload features
    //


    // So enable them and try again
    toggle_features();
    write_register(FEATURE, read_register(FEATURE) | _BV(EN_ACK_PAY) | _BV(EN_DPL));

    if (debug)
        printf("FEATURE=%i\r\n", read_register(FEATURE));

    //
    // Enable dynamic payload on pipes 0 & 1
    //
    dynamic_payloads_enabled = true;
    write_register(DYNPD, read_register(DYNPD) | _BV(DPL_P1) | _BV(DPL_P0));
}

/****************************************************************************/

void rf24_enableDynamicAck(void){
    //
    // enable dynamic ack features
    //
    toggle_features();
    write_register(FEATURE, read_register(FEATURE) | _BV(EN_DYN_ACK));

    if (debug) {
        printf("FEATURE=%i\r\n", read_register(FEATURE));
    }
}

/****************************************************************************/

void rf24_writeAckPayload(uint8_t pipe, const void* buf, uint8_t len)
{

    uint8_t *ptx = spi_txbuff;
    uint8_t size;

    const uint8_t *current = (const uint8_t *)(buf);

    uint8_t data_len = min(len, payload_size);

    size = data_len + 1; // Add register value to transmit buffer

    if (debug){
        printf("[Writing %u bytes]", data_len);
    }
    *ptx++ =  W_ACK_PAYLOAD | (pipe & 0b111);
    while (data_len--) {
        *ptx++ =  *current++;
    }
    bcm2835_spi_transfern((char *)spi_txbuff, size);	

}


/****************************************************************************/

bool rf24_isAckPayloadAvailable(void)
{
    return !read_register(FIFO_STATUS) & _BV(RX_EMPTY);
}

/****************************************************************************/

bool rf24_isPVariant(void)
{
    return p_variant;
}

/****************************************************************************/

void rf24_setAutoAck(bool enable)
{
    if (enable)
        write_register(EN_AA, 0b111111);
    else
        write_register(EN_AA, 0);
}

/****************************************************************************/

void rf24_setAutoAck_pipe(uint8_t pipe, bool enable)
{
    if (pipe <= 6) {
        uint8_t en_aa = read_register(EN_AA);
        if (enable) {
            en_aa |= _BV(pipe);
        } else {
            en_aa &= ~_BV(pipe);
        }
        write_register(EN_AA, en_aa);
    }
}

/****************************************************************************/

bool rf24_testCarrier(void)
{
    return (read_register(CD) & 1);
}

/****************************************************************************/

bool rf24_testRPD(void)
{
    return (read_register(RPD) & 1);
}

/****************************************************************************/

bool rf24_isValid(void)
{
    return ce_pin != 0xff && csn_pin != 0xff;
}

/****************************************************************************/

void rf24_setPALevel(uint8_t level)
{
    uint8_t setup = read_register(RF_SETUP) & 0b11111000;

    if (level > 3) {  						// If invalid level, go to max PA
        level = (RF24_PA_MAX << 1) + 1;		// +1 to support the SI24R1 chip extra bit
    } else {
        level = (level << 1) + 1;	 		// Else set level as requested
    }


    write_register(RF_SETUP, setup |= level);	// Write it to the chip
}

/****************************************************************************/

uint8_t rf24_getPALevel(void)
{
    return (read_register(RF_SETUP) & (_BV(RF_PWR_LOW) | _BV(RF_PWR_HIGH))) >> 1;
}

/****************************************************************************/

bool rf24_setDataRate(rf24_datarate_e speed)
{
    bool result = false;
    uint8_t setup = read_register(RF_SETUP);

    // HIGH and LOW '00' is 1Mbs - our default
    setup &= ~(_BV(RF_DR_LOW) | _BV(RF_DR_HIGH));
    if (speed == RF24_250KBPS) {
        // Must set the RF_DR_LOW to 1; RF_DR_HIGH (used to be RF_DR) is already 0
        // Making it '10'.
        setup |= _BV(RF_DR_LOW);
    } else {
        // Set 2Mbs, RF_DR (RF_DR_HIGH) is set 1
        // Making it '01'
        if (speed == RF24_2MBPS) {
            setup |= _BV(RF_DR_HIGH);
        }
    }
    write_register(RF_SETUP, setup);

    // Verify our result
    if (read_register(RF_SETUP) == setup) {
        result = true;
    }

    return result;
}

/****************************************************************************/

rf24_datarate_e rf24_getDataRate(void)
{
    rf24_datarate_e result;
    uint8_t dr = read_register(RF_SETUP) & (_BV(RF_DR_LOW) | _BV(RF_DR_HIGH));

    // switch uses RAM (evil!)
    // Order matters in our case below
    if (dr == _BV(RF_DR_LOW)) {
        // '10' = 250KBPS
        result = RF24_250KBPS;
    } else if (dr == _BV(RF_DR_HIGH)) {
        // '01' = 2MBPS
        result = RF24_2MBPS;
    } else {
        // '00' = 1MBPS
        result = RF24_1MBPS;
    }
    return result;
}

/****************************************************************************/

void rf24_setCRCLength(rf24_crclength_e length)
{
    uint8_t config = read_register(CONFIG) & ~(_BV(CRCO) | _BV(EN_CRC));

    // switch uses RAM (evil!)
    if (length == RF24_CRC_DISABLED) {
        // Do nothing, we turned it off above.
    } else if (length == RF24_CRC_8) {
        config |= _BV(EN_CRC);
    } else {
        config |= _BV(EN_CRC);
        config |= _BV(CRCO);
    }
    write_register(CONFIG, config);
}

/****************************************************************************/

rf24_crclength_e rf24_getCRCLength(void)
{
    rf24_crclength_e result = RF24_CRC_DISABLED;
    uint8_t config = read_register(CONFIG) & (_BV(CRCO) | _BV(EN_CRC));

    if (config & _BV(EN_CRC)) {
        if (config & _BV(CRCO))
            result = RF24_CRC_16;
        else
            result = RF24_CRC_8;
    }

    return result;
}

/****************************************************************************/

void rf24_disableCRC(void)
{
    uint8_t disable = read_register(CONFIG) & ~_BV(EN_CRC);
    write_register(CONFIG, disable);
}

/****************************************************************************/
void rf24_setRetries(uint8_t delay, uint8_t count)
{
    write_register(SETUP_RETR, (delay & 0xf) << ARD | (count & 0xf) << ARC);
}
