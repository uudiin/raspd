/*
   Copyright (C) 2011 J. Coliz <maniacbug@ymail.com>

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License
   version 2 as published by the Free Software Foundation.

   03/17/2013 : Charles-Henri Hallard (http://hallard.me)
   Modified to use with Arduipi board http://hallard.me/arduipi
   Changed to use modified bcm2835 and RF24 library
   TMRh20 2014 - Updated to work with optimized RF24 Arduino library

*/

/**
 * Example RF Radio Ping Pair
 *
 * This is an example of how to use the RF24 class on RPi, communicating to an Arduino running
 * the GettingStarted sketch.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <assert.h>

#include <nrf24.h>

#include "../raspd/event.h"
#include "../raspd/gpiolib.h"

static int irq_pin = 18;
static unsigned long magic = 0;

// Radio pipe addresses for the 2 nodes to communicate.
const uint8_t pipes[][6] = {"1Node","2Node"};

void tx_test(void)
{
    rf24_openWritingPipe(pipes[0]);

    // forever loop
    while (1) {
        // Take the time, and send it.  This will block until complete

        printf("Now sending...\n");
        magic++;
        bool ok = rf24_write(&magic, sizeof(unsigned long));
        if (!ok){
            printf("failed.\n");
        }

        sleep(1);

    } // forever loop
}

static void gpio_interrupt(int fd, short what, void *arg)
{
    if (rf24_available()) {

        // Fetch the payload, and see if this was the last one.
        rf24_read(&magic, sizeof(unsigned long));
        printf("Got response %lu\n", magic);
    }
}

void rx_test(void)
{
    int err;

    rf24_openReadingPipe(1, pipes[0]);
    rf24_startListening();

    gpiolib_init();

    err = rasp_event_init();
    assert(err >= 0);

    err = bcm2835_gpio_signal(irq_pin, EDGE_falling, gpio_interrupt, (void *)irq_pin, NULL);
    assert(err >= 0);

    err = rasp_event_loop();
    assert(err == 0);

    rasp_event_exit();

    gpiolib_exit();
}


int main(int argc, char **argv)
{
    int c;

    if (argc < 2)
        return 1;

    c = atoi(argv[1]);

    // Print preamble:
    printf("RF24/examples/pingtest/\n");

    // CE Pin, CSN Pin, SPI Speed
    // Setup for GPIO 22 CE and CE0 CSN with SPI Speed @ 8Mhz
    RF24(23, 8, BCM2835_SPI_SPEED_8MHZ);

    // Setup and configure rf radio
    rf24_begin();

    // optionally, increase the delay between retries & # of retries
    rf24_setRetries(15, 15);
    // Dump the configuration of the rf unit for debugging
    rf24_printDetails();


    /********* Role chooser ***********/

    printf("\n ************ Role Setup ***********\n");

    if (c == 0){
        printf("Role: Pong Back, awaiting transmission \n\n");
        rx_test();
    } else {
        printf("Role: Ping Out, starting transmission \n\n");
        tx_test();
    }

    return 0;
}

