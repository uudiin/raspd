#include <string.h>
#include <stdio.h>
#include <unistd.h>

#include "client.h"

void cbrecv(char *buf, int buflen)
{
	printf("%s", buf);
}

int main(int argc, char *argv[])
{
	int err;

	err = client_connect("127.0.0.1", 8888);
	if (err < 0) {
		printf("connect_stream err = %d\n", err);
		return err;
	}

	client_msg_dispatch(cbrecv);

	speed_up();
	speed_down();
	up();
	down();
	left();
	right();
	usleep(9000000);

	return 0;
}
