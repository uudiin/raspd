#include <string.h>
#include <stdio.h>

int main(int argc, char *argv[])
{
	int err;
	char buf[1024] = "\0";

	err = connect_stream("127.0.0.1", 8888);
	if (err < 0) {
		printf("connect_stream err = %d\n", err);
		return err;
	}

	send_cmd("gpio: xxx", strlen("gpio: xxx"));

	recv_response(buf, 1024);
	printf("%s\n", buf);

	return 0;
}
