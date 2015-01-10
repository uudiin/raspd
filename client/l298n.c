#include <stdio.h>
#include <string.h>


#include "client.h"

static int select = 0;

static void l298n_up(void)
{
	char cmd1[] = "l298n_lup;";
	char cmd2[] = "l298n_rup;";
	client_send_cmd(cmd1, strlen(cmd1));
	client_send_cmd(cmd2, strlen(cmd2));
}

static void l298n_down(void)
{
	char cmd1[] = "l298n_ldown;";
	char cmd2[] = "l298n_rdown;";
	client_send_cmd(cmd1, strlen(cmd1));
	client_send_cmd(cmd2, strlen(cmd2));
}

static void l298n_left(void)
{
	char cmd1[] = "l298n_ldown;";
	char cmd2[] = "l298n_rup;";
	client_send_cmd(cmd1, strlen(cmd1));
	client_send_cmd(cmd2, strlen(cmd2));
} 

static void l298n_right(void)
{
	char cmd1[] = "l298n_lup;";
	char cmd2[] = "l298n_rdown;";
	client_send_cmd(cmd1, strlen(cmd1));
	client_send_cmd(cmd2, strlen(cmd2));
} 

static void l298n_brake(void)
{
	char cmd1[] = "l298n_lbrake;";
	char cmd2[] = "l298n_rbrake;";
	client_send_cmd(cmd1, strlen(cmd1));
	client_send_cmd(cmd2, strlen(cmd2));
} 

static void l298n_speed_up(void)
{
	char cmd1[] = "l298n_lspeedup;";
	char cmd2[] = "l298n_rspeedup;";
	client_send_cmd(cmd1, strlen(cmd1));
	client_send_cmd(cmd2, strlen(cmd2));
}

static void l298n_speed_down(void)
{
	char cmd1[] = "l298n_lspeeddown;";
	char cmd2[] = "l298n_rspeeddown;";
	client_send_cmd(cmd1, strlen(cmd1));
	client_send_cmd(cmd2, strlen(cmd2));
}

static void tank_up(void)
{
	char cmd1[] = "tank_fwd;";
	client_send_cmd(cmd1, strlen(cmd1));
}

static void tank_down(void)
{
	char cmd1[] = "tank_rev;";
	client_send_cmd(cmd1, strlen(cmd1));
}

static void tank_left(void)
{
	char cmd1[] = "tank_left;";
	client_send_cmd(cmd1, strlen(cmd1));
}

static void tank_right(void)
{
	char cmd1[] = "tank_right;";
	client_send_cmd(cmd1, strlen(cmd1));
}

static void tank_brake(void)
{
	char cmd1[] = "tank_brake;";
	client_send_cmd(cmd1, strlen(cmd1));
}

static void tank_speed_up(void)
{
	char cmd1[] = "tank_sup;";
	client_send_cmd(cmd1, strlen(cmd1));
}

static void tank_speed_down(void)
{
	char cmd1[] = "tank_sdown;";
	client_send_cmd(cmd1, strlen(cmd1));
}

void select_tank(void)
{
	select = 1;
}

void up(void)
{
	select ? tank_up() : l298n_up();
}

void down(void)
{
	select ? tank_down() : l298n_down();
}

void left(void)
{
	select ? tank_left() : l298n_left();
}

void right(void)
{
	select ? tank_right() : l298n_right();
}

void brake(void)
{
	select ? tank_brake() : l298n_brake();
}

void speed_up(void)
{
	select ? tank_speed_up() : l298n_speed_up();
}

void speed_down(void)
{
	select ? tank_speed_down() : l298n_speed_down();
}

void turret_left(void)
{
	char cmd1[] = "tank_turret_left;";
	client_send_cmd(cmd1, strlen(cmd1));
}

void turret_right(void)
{
	char cmd1[] = "tank_turret_right;";
	client_send_cmd(cmd1, strlen(cmd1));
}

void turret_elev(void)
{
	char cmd1[] = "tank_turret_elev;";
	client_send_cmd(cmd1, strlen(cmd1));
}

void fire(void)
{
	char cmd1[] = "tank_fire;";
	client_send_cmd(cmd1, strlen(cmd1));
}
