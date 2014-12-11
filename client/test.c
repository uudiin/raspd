#include <string.h>
#include <stdio.h>

void cbrecv(char *buf, int buflen)
{
	printf(buf);
}

int main(int argc, char *argv[])
{
	int err;
	char buf[1024] = "\0";

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
