#ifndef __RPI_CLIENT_H__
#define __RPI_CLIENT_H__

int send_cmd(char *buf, int bufsize);
int recv_response(char *buf, int bufsize);
int connect_stream(const char *hostname, unsigned short portno);

void up(void);
void down(void);
void left(void);
void right(void);
void speed_up(void);
void speed_down(void);

#endif
