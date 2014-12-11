#include <stdio.h>
#include <string.h>

void up(void)
{
	char cmd1[] = "l298n_lup;";
	char cmd2[] = "l298n_rup;";
	client_send_cmd(cmd1, strlen(cmd1));
	client_send_cmd(cmd2, strlen(cmd2));
}

void down(void)
{
	char cmd1[] = "l298n_ldown;";
	char cmd2[] = "l298n_rdown;";
	client_send_cmd(cmd1, strlen(cmd1));
	client_send_cmd(cmd2, strlen(cmd2));
}

void left(void)
{
	char cmd1[] = "l298n_ldown;";
	char cmd2[] = "l298n_rup;";
	client_send_cmd(cmd1, strlen(cmd1));
	client_send_cmd(cmd2, strlen(cmd2));
} 

void right(void)
{
	char cmd1[] = "l298n_lup;";
	char cmd2[] = "l298n_rdown;";
	client_send_cmd(cmd1, strlen(cmd1));
	client_send_cmd(cmd2, strlen(cmd2));
} 

void brake(void)
{
	char cmd1[] = "l298n_lbrake;";
	char cmd2[] = "l298n_rbrake;";
	client_send_cmd(cmd1, strlen(cmd1));
	client_send_cmd(cmd2, strlen(cmd2));
} 

void speed_up(void)
{
	char cmd1[] = "l298n_lspeedup;";
	char cmd2[] = "l298n_rspeedup;";
	client_send_cmd(cmd1, strlen(cmd1));
	client_send_cmd(cmd2, strlen(cmd2));
}

void speed_down(void)
{
	char cmd1[] = "l298n_lspeeddown;";
	char cmd2[] = "l298n_rspeeddown;";
	client_send_cmd(cmd1, strlen(cmd1));
	client_send_cmd(cmd2, strlen(cmd2));
}
