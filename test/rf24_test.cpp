#include <RF24.h>

RF24 radio(RPI_BPLUS_GPIO_J8_24, RPI_BPLUS_GPIO_J8_16, 0);

int main(int argc, char **argv)
{
	radio.begin();

	return 0;
}
