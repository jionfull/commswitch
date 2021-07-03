/*
 * port_manager.c
 *
 *  Created on: Dec 3, 2013
 *      Author: lifeng
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "config.h"
#include "network.h"
#include "gather_port.h"
#include "can_port.h"
#include "port_manager.h"

static char *get_sys_port(char * port);
static void insert_can_port(struct port_manager* manager, struct can_port *port);
static void insert_gather_port(struct port_manager* manager,
		struct gather_port *port);

static struct port_manager * manager_global = NULL;

struct port_manager * get_port_manager() {
	if (manager_global == NULL) {
		int i, j;
		manager_global = (struct port_manager*) malloc(
				sizeof(struct port_manager));

		bzero(manager_global, sizeof(struct port_manager));

		struct config_global * config = get_global_config();

		manager_global->config = config;

		//network
		manager_global->network = create_network(config->networkPort);

		//CAN,RS485 Ports
		int portNum = config->portNum;

		for (i = 0; i < portNum; i++) {
			struct port_config* port = config->ports + i;

			if (strncmp(port->type, "CAN", 3) == 0) //CAN数据
					{
				struct can_port* canPort = create_can_port(port->name,
						port->baudrate);
				insert_can_port(manager_global, canPort);
			} else if (strncmp(port->type, "RS", 2) == 0) {
				char * serial = get_sys_port(port->name);

				if (port->workMode == 2) {//smart sensor II型智能传感器
					struct gather_port* gather_port = create_gather(serial,
							port->baudrate);
					if (gather_port != NULL) {

						gather_port->portIndex = atoi(port->name + 3);

						for (j = 0; j < port->sensorNum; j++)  //创建智能传感器
								{
							add_sensor_II(gather_port, port->sensors[j].addr,
									port->sensors[j].type);
						}

						insert_gather_port(manager_global, gather_port);
					}

				}

				else if (port->workMode == 3) //modbus 透传
						{

					struct gather_port* gather_port = create_modbus(serial,
							port->baudrate);
					if (gather_port != NULL) {
						gather_port->portIndex = atoi(port->name + 3);
						insert_gather_port(manager_global, gather_port);
					}

				} else if (port->workMode == 4) //DLT485 透传
						{

					struct gather_port* gather_port = create_dlt645(serial,
							port->baudrate);
					if (gather_port != NULL) {
						gather_port->portIndex = atoi(port->name + 3);
						insert_gather_port(manager_global, gather_port);
					}

				}
			}
		}

	}
	return manager_global;
}

void start_port_manager(struct port_manager * manager) {
	if (manager == NULL) {
		printf("manager == NULL");
		return;
	}
	if (manager->gather_num == 0) {
		printf("manager->gather_num==0");

	}

	int i;
	if (manager->network != NULL) {
		start_network(manager->network);
	}

	for (i = 0; i < manager->can_num; i++) {
		start_can_port(manager->can_ports[i]);
	}
	for (i = 0; i < manager->gather_num; i++) {
		start_gather_port(manager->gathers[i]);
	}

}

void stop_port_manager(struct port_manager * manager) {

}

void send_network_data(struct port_manager * manager, char* buffer, int offset,
		int length) {
	if (manager == NULL)
		return;

	network_send(manager->network, buffer, offset, length);
}

/*
 * data struct :byte[0]-portIndex, byte[1-4] can-ID ,byte[5]-data-length ,byte[6-N] can-data
 * */
void to_can_data(struct port_manager* manager, char* buffer, int length) {
	char portIndex = buffer[0];

	int i;
	for (i = 0; i < manager->can_num; i++) {
		if (manager->can_ports[i]->portIndex == portIndex) {
			send_can_data(manager->can_ports[i], buffer + 1);
			break;
		}
	}

}

void get_port_mode(struct port_manager*manager, char cmd, char *buffer,
		int length) {
	char portIndex = buffer[0];
	char frame[11];
	int i;
	char status = 0xff;
	frame[0] = cmd; //Serial-status
	frame[1] = portIndex; //COM Num

	frame[2] = 0;
	frame[3] = 0;
	frame[4] = 0;
	frame[5] = 0;

	frame[6] = 0;
	frame[7] = 0;
	frame[8] = 0;
	frame[9] = 0;

	for (i = 0; i < manager->gather_num; i++) {
		if (manager->gathers[i]->portIndex == portIndex) {
			status = (char) (manager->gathers[i]->work_mode);
			break;
		}
	}
	frame[10] = status;

	send_network_data(manager, frame, 0, 11);
}

void set_port_mode(struct port_manager*manager, char *buffer, int length) {
	char portIndex = buffer[0];

	int i;
	for (i = 0; i < manager->gather_num; i++) {
		if (manager->gathers[i]->portIndex == portIndex) {
			manager->gathers[i]->work_mode = buffer[3];
			manager->gathers[i]->cmd_length = 0;
			manager->gathers[i]->cmd_start_index = 0;
			break;
		}
	}
	get_port_mode(manager, 0xf2, buffer, length);
}
void to_serial_data(struct port_manager*manager, char *buffer, int length) {
	char portIndex = buffer[0];

	int i;
	for (i = 0; i < manager->gather_num; i++) {

		if (manager->gathers[i]->portIndex == portIndex) {
			printf("find port %x\n",portIndex);
			send_serial_data(manager->gathers[i], buffer + 1, length - 1);

			return;
		}
	}
	printf("not find port %x\n",portIndex);

}

static char *get_sys_port(char * port) {
	static char* sys_port[] = { "/dev/ttyO1", "/dev/ttyO2", "/dev/ttyO3",
			"/dev/ttyO4", "/dev/ttyO5" };
	static char* board_port[] = { "COM1", "COM2", "COM3", "COM4", "COM5" };
	int i;
	char * name = NULL;
	for (i = 0; i < 5; i++) {
		if (strcmp(port, board_port[i]) == 0) {
			name = sys_port[i];
			break;
		}
	}
	return name;
}

static void insert_can_port(struct port_manager* manager, struct can_port *port) {

	if (manager == NULL)
		return;
	if (manager->can_num >= MAX_CAN_NUM)
		return;

	manager->can_ports[manager->can_num] = port;
	manager->can_num = manager->can_num + 1;

}

static void insert_gather_port(struct port_manager* manager,
		struct gather_port *port) {

	if (manager == NULL)
		return;
	if (manager->gather_num >= MAX_GATHER_NUM)
		return;

	manager->gathers[manager->gather_num] = port;
	manager->gather_num = manager->gather_num + 1;

}

