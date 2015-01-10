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

#include <RF24.h>
using namespace std;
//
// Hardware configuration
//

// CE Pin, CSN Pin, SPI Speed
// Setup for GPIO 22 CE and CE0 CSN with SPI Speed @ 8Mhz
RF24 radio(23, 8, BCM2835_SPI_SPEED_8MHZ);


// Radio pipe addresses for the 2 nodes to communicate.
const uint8_t pipes[][6] = {"1Node","2Node"};

int main(int argc, char **argv)
{
  bool role_ping_out = true, role_pong_back = false;
  bool role = role_pong_back;
  int c;

  if (argc < 2)
	  return 1;

	c = atoi(argv[1]);

  // Print preamble:
  printf("RF24/examples/pingtest/\n");

  // Setup and configure rf radio
  radio.rf24_begin();

  // optionally, increase the delay between retries & # of retries
  radio.rf24_setRetries(15,15);
  // Dump the configuration of the rf unit for debugging
  radio.rf24_printDetails();


/********* Role chooser ***********/

  printf("\n ************ Role Setup ***********\n");
  //string input = "";
  //char myChar = {0};
  //cout << "Choose a role: Enter 0 for pong_back, 1 for ping_out (CTRL+C to exit) \n>";
  //getline(cin,input);

	if(c == 0){
		printf("Role: Pong Back, awaiting transmission ");
	}else{  printf("Role: Ping Out, starting transmission ");
		role = role_ping_out;
	}

/***********************************/
  // This simple sketch opens two pipes for these two nodes to communicate
  // back and forth.

    if ( role == role_ping_out )    {
      radio.rf24_openWritingPipe(pipes[0]);
      radio.rf24_openReadingPipe(1,pipes[1]);
    } else {
      radio.rf24_openWritingPipe(pipes[1]);
      radio.rf24_openReadingPipe(1,pipes[0]);
      radio.rf24_startListening();

    }
	
	// forever loop
	while (1)
	{
		if (role == role_ping_out)
		{
			// First, stop listening so we can talk.
			radio.rf24_stopListening();

			// Take the time, and send it.  This will block until complete

			printf("Now sending...\n");
			unsigned long time = 8888;

			bool ok = radio.rf24_write( &time, sizeof(unsigned long) );

			if (!ok){
				printf("failed.\n");
			}
			// Now, continue listening
			radio.rf24_startListening();

			while ( ! radio.rf24_available()) {
			}

		//		// Grab the response, compare, and send to debugging spew
				unsigned long got_time;
				radio.rf24_read( &got_time, sizeof(unsigned long) );

				// Spew it
				printf("Got response %lu\n",got_time);

			// Try again 1s later
			// delay(1000);

			sleep(1);

		}

		//
		// Pong back role.  Receive each packet, dump it out, and send it back
		//

		if ( role == role_pong_back )
		{
			
			// if there is data ready
			//printf("Check available...\n");

			if ( radio.rf24_available() )
			{
				// Dump the payloads until we've gotten everything
				unsigned long got_time;


				// Fetch the payload, and see if this was the last one.
				radio.rf24_read( &got_time, sizeof(unsigned long) );
				printf("Got response %lu\n",got_time);

				radio.rf24_stopListening();
				
				radio.rf24_write( &got_time, sizeof(unsigned long) );

				// Now, resume listening so we catch the next packets.
				radio.rf24_startListening();

				// Spew it
				printf("Got payload(%d) %lu...\n",sizeof(unsigned long), got_time);
				
				delay(925); //Delay after payload responded to, minimize RPi CPU time
				
			}
		
		}

	} // forever loop

  return 0;
}

