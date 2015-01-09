#include <string.h>
#include <stdio.h>
#include <unistd.h>

#include <nrf24.h>

//
// Hardware configuration
//

// CE Pin, CSN Pin, SPI Speed

// Setup for GPIO 22 CE and CE1 CSN with SPI Speed @ 1Mhz
//RF24 radio(RPI_V2_GPIO_P1_22, RPI_V2_GPIO_P1_26, BCM2835_SPI_SPEED_1MHZ);

// Setup for GPIO 22 CE and CE0 CSN with SPI Speed @ 4Mhz
//RF24 radio(RPI_V2_GPIO_P1_15, BCM2835_SPI_CS0, BCM2835_SPI_SPEED_4MHZ);

// Setup for GPIO 22 CE and CE0 CSN with SPI Speed @ 8Mhz
//RF24 radio(23, 8, BCM2835_SPI_SPEED_8MHZ);


// Radio pipe addresses for the 2 nodes to communicate.
uint8_t pipes[][6] = {"1Node", "2Node"};

//const uint64_t pipes[2] = { 0xABCDABCD71LL, 0x544d52687CLL };


int main(int argc, char* argv[]){

	int role_ping_out = 1, role_pong_back = 0;
	int role = role_pong_back;
	int c;

	if (argc < 2)
		return 1;

	c = atoi(argv[1]);

	RF24(23, 8, 8);

	// Print preamble:
	printf("RF24/examples/pingtest/\n");

	// Setup and configure rf radio
	begin();

	// optionally, increase the delay between retries & # of retries
	setRetries(15,15);
	// Dump the configuration of the rf unit for debugging
	printDetails();


	/********* Role chooser ***********/

	printf("\n ************ Role Setup ***********\n");

	printf("Choose a role: Enter 0 for pong_back, 1 for ping_out (CTRL+C to exit) \n>");

	if(c == '0'){
		printf("Role: Pong Back, awaiting transmission \n\n");
	}else{
		printf("Role: Ping Out, starting transmission \n\n");
		role = role_ping_out;
	}
	/***********************************/
	// This simple sketch opens two pipes for these two nodes to communicate
	// back and forth.

	if ( role == role_ping_out )    {
		openWritingPipe(pipes[0]);
		openReadingPipe(1,pipes[1]);
	} else {
		openWritingPipe(pipes[1]);
		openReadingPipe(1,pipes[0]);
		startListening();
	}

	// forever loop
	while (1)
	{
		if (role == role_ping_out)
		{
			// First, stop listening so we can talk.
			stopListening();

			// Take the time, and send it.  This will block until complete

			printf("Now sending...\n");
			unsigned long time = millis();

			int ok = rf24_write( &time, sizeof(unsigned long), 0 );

			if (!ok){
				printf("failed.\n");
			}
			// Now, continue listening
			startListening();

			// Wait here until we get a response, or timeout (250ms)
			unsigned long started_waiting_at = millis();
			int timeout = 0;
			while ( ! available() && ! timeout ) {
				if (millis() - started_waiting_at > 200 )
					timeout = 1;
			}


			// Describe the results
			if ( timeout )
			{
				printf("Failed, response timed out.\n");
			}
			else
			{
				// Grab the response, compare, and send to debugging spew
				unsigned long got_time;
				rf24_read( &got_time, sizeof(unsigned long) );

				// Spew it
				printf("Got response %lu, round-trip delay: %lu\n",got_time,millis()-got_time);
			}

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

			if ( available() )
			{
				// Dump the payloads until we've gotten everything
				unsigned long got_time;


				// Fetch the payload, and see if this was the last one.
				rf24_read( &got_time, sizeof(unsigned long) );

				stopListening();

				rf24_write2( &got_time, sizeof(unsigned long));

				// Now, resume listening so we catch the next packets.
				startListening();

				// Spew it
				printf("Got payload(%d) %lu...\n",sizeof(unsigned long), got_time);

				delay(925); //Delay after payload responded to, minimize RPi CPU time

			}

		}

	} // forever loop

	return 0;
}
