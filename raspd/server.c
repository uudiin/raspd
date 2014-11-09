#include <sock.h>

int listen_stream(unsigned short portno)
{
	union sockaddr_u addr;
	size_t ss_len;
	int fd_listen;
	int err;

	ss_len = sizeof(addr);
	err = resolve("0.0.0.0", portno, &addr.storage, &ss_len, AF_INET, 0);
	if (err < 0)
		return err;

	fd_listen = do_listen(SOCK_STREAM, IPPROTO_TCP, &addr);
	if (fd_listen == -1)
		return -1;

	/* recv loop */
	while(1) {
		union sockaddr_u remoteaddr;
		socklen_t ss_len;
		int fd;

		fd = accept(fd_listen, &remoteaddr.sockaddr, &ss_len);
		if (fd < 0)
			return;

		if (fork() != 0){
			/* FIXME: close(fd) ?? */
			continue;
		}

		do_raspd(fd);
		
	}

	close(fd_listen);

	return 0;
}
