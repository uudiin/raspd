#include <stdio.h>
#include <string.h>

void up(void)
{
	char cmd[] = "l298n --lup;";
	send_cmd(cmd, strlen(cmd));
	cmd[8] = 'r';
	send_cmd(cmd, strlen(cmd));
}

void down(void)
{
	char cmd[] = "l298n --ldown;";
	send_cmd(cmd, strlen(cmd));
	cmd[8] = 'r';
	send_cmd(cmd, strlen(cmd));
}

void left(void)
{
	char cmd[] = "l298n --ldown;";
	char cmd2[] = "l298n --rup;";
	send_cmd(cmd, strlen(cmd));
	send_cmd(cmd2, strlen(cmd2));
} 

void right(void)
{
	char cmd[] = "l298n --lup;";
	char cmd2[] = "l298n --rdown;";
	send_cmd(cmd, strlen(cmd));
	send_cmd(cmd2, strlen(cmd2));
} 

void brake(void)
{
	char cmd[] = "l298n --lbrake;";
	char cmd2[] = "l298n --rbrake;";
	send_cmd(cmd, strlen(cmd));
	send_cmd(cmd2, strlen(cmd2));
}

static speed = 0;
void speed_up(void)
{
	char cmd1[64];
	char cmd2[64];
	speed++;
 	sprintf(cmd1, "l298n --lspeed %d;", speed);
 	sprintf(cmd2, "l298n --rspeed %d;", speed);
	send_cmd(cmd1, strlen(cmd1));
	send_cmd(cmd2, strlen(cmd2));
	
}

void speed_down(void)
{
	char cmd1[64];
	char cmd2[64];
	speed--;
 	sprintf(cmd1, "l298n --lspeed %d;", speed);
 	sprintf(cmd2, "l298n --rspeed %d;", speed);
	send_cmd(cmd1, strlen(cmd1));
	send_cmd(cmd2, strlen(cmd2));
	
}
