#ifndef __RPI_CLIENT_H__
#define __RPI_CLIENT_H__

int client_send_cmd(char *buf, int bufsize);
int client_connect(const char *hostname, unsigned short portno);
int client_msg_dispatch(void (*cb)(char *buf, int buflen));

void up(void);
void down(void);
void left(void);
void right(void);
void speed_up(void);
void speed_down(void);

#endif
