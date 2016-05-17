/*
 * gather.h
 *
 * one serial port to one gather
 *
 *  Created on: Dec 2, 2013
 *      Author: lifeng
 */

#ifndef GATHER_H_
#define GATHER_H_

#include <pthread.h>
#include "frame_manager.h"
#include "config.h"

#define MAX_SENSOR	(33)
#define MAX_CMD_COUNT	(2)
#define MAX_SENSOR_CMD_LENGTH	(1024)

#define MAX_GATHER_TX_LENGTH	(64*1024)
#define MAX_GATHER_RX_LENGTH	(32*1024)

struct smart_sensor {
	struct gather_port * port;
	int addr;
	int timeout_count;
	int wait_time; /*wait for sensor boot to app*/
	char *tx_data;
	char *rx_data;
	char version[6];

	/*command process*/
	pthread_mutex_t mutext;
	int cmd_start_index;
	int cmd_end_index;
	char cmd_list[MAX_CMD_COUNT][MAX_SENSOR_CMD_LENGTH];

	struct sensor_type * type;
	void (*query_digit)(struct smart_sensor *sensor);
	void (*query_analog)(struct smart_sensor *sensor);
	void (*query_curve)(struct smart_sensor *sensor);
	void (*query_others)(struct smart_sensor *sensor);

};

enum GatherMode {
	MODE_DEBUG = 1, MODE_WORK = 2,
};

struct gather_port {
	int used;
	int serial_fd;
	int tx_led_fd;
	int rx_led_fd;
	char serial_name[50];
	char portIndex;
	int baudrate;

	char tx_data[MAX_GATHER_TX_LENGTH];
	char rx_data[MAX_GATHER_RX_LENGTH];
	int sensor_num;
	struct smart_sensor sensors[MAX_SENSOR];
	int last_badsensor;
	struct frame_manager * frame_manager;
	pthread_t thread_wk;

	pthread_mutex_t mutext;
	int cmd_start_index;
	int cmd_length;
	char cmd_list[MAX_CMD_COUNT][MAX_GATHER_TX_LENGTH];
	enum GatherMode work_mode;

};

struct gather_port* create_gather(char*serial_name, int baudrate);

void gather_add_module(int addr, int type);

void start_gather_port(struct gather_port* pgather);
void stop_gather_port(struct gather_port* pgather);

void add_sensor_II(struct gather_port *port, int addr, int type);

void send_serial_data(struct gather_port *port, char * buffer, int length);

#endif /* GATHER_H_ */
