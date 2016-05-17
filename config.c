/*
 * config.c
 *
 *  Created on: Dec 3, 2013
 *      Author: lifeng
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>

#include "ini_doc.h"
#include "config.h"

static struct sensor_type * inser_sensor_type(struct config_global * gconfig,
		int type, int mode, int time);
static struct port_config* insert_port_config(struct config_global * gconfig,
		char* name, char* type, int baudrate, int workMode);
static void insert_sensor_config(struct port_config * port, int addr, int type);

static struct config_global *global_config = NULL;

extern const char const config_begin[];
extern const char const config_end[];

static void insert_sensor_config(struct port_config * port, int addr, int type) {
	if (port == NULL)
		return;

	if (port->sensorNum >= MAX_SENSORS_PER_PORT)
		return;

	struct sensor_config * pSensor = port->sensors + (port->sensorNum);

	pSensor->addr = addr;
	pSensor->type = type;
	port->sensorNum = port->sensorNum + 1;

}

static void insert_sensors_segment(struct port_config * pPort, char *segment,
		int startIndex, int midIndex) {

	int i;
	int startNum, endNum;
	if (midIndex > 0) {
		startNum = atoi(segment + startIndex);
		endNum = atoi(segment + midIndex + 1);

	} else {
		startNum = atoi(segment + startIndex);
		endNum = startNum;
	}
	for (i = startNum; i <= endNum; i++) {

		insert_sensor_config(pPort, i, -1);
	}
}

static void insert_sensors_list(struct port_config * pPort, char *list) {
	char sensorList[100];
	int startIndex, endIndex, midIndex;

	strcpy(sensorList, list);

	startIndex = 0;
	endIndex = 0;
	midIndex = -1;

	while (sensorList[endIndex] != '\0') {
		if (sensorList[endIndex] == ',') {
			sensorList[endIndex] = '\0';

			if (endIndex > startIndex) {

				insert_sensors_segment(pPort, sensorList, startIndex, midIndex);

				midIndex = -1;
			}
			startIndex = endIndex + 1;
		} else if (sensorList[endIndex] == '-') {
			sensorList[endIndex] = '\0';
			midIndex = endIndex;
		}
		endIndex++;
	}
	if (endIndex > startIndex) {

		insert_sensors_segment(pPort, sensorList, startIndex, midIndex);
		midIndex = -1;
	}
}

struct sensor_type * get_sensor_type(int type) {
	int i, num;
	if (global_config == NULL)
		return NULL;
	num = global_config->sensor_num;
	struct sensor_type * s_type = NULL;

	for (i = 0; i < num; i++) {
		if (global_config->sensor_types[i].type == type) {
			s_type = global_config->sensor_types + i;
			break;
		}
	}
	return s_type;
}
struct config_global * get_global_config() {
	if (global_config == NULL) {
		int i;

		char buffer[128];


		if ((access("./config.ini", F_OK)) == -1) // file does not exist!
		{

			FILE* file=fopen("./config.ini","wb");
			if(file!=NULL)
			{
				fwrite(config_begin,config_end-config_begin,1,file);
				fflush(file);
				fclose(file);
			}

		}
		global_config = (struct config_global*) malloc(
				sizeof(struct config_global));
		struct ini_doc * config_doc = create_ini_doc("./config.ini");
		global_config->pDoc = config_doc;
		//[General]
		int portNum = get_ini_int(config_doc, "General", "PortNum");
		global_config->portNum = 0; //portNum;
		global_config->sensor_num = 0; //sensorNUM
		int networkPort = get_ini_int(config_doc, "General", "NetworkPort");
		global_config->networkPort = networkPort;
		int sensor_num = get_ini_int(config_doc, "General", "SensorNum");
		//[Sensor*]
		for (i = 1; i <= sensor_num; i++) {
			sprintf(buffer, "Sensor%d", i);
			int type = get_ini_int(config_doc, buffer, "Type");
			int mode = get_ini_int(config_doc, buffer, "QueryMode");
			int wTime = get_ini_int(config_doc, buffer, "WaitTime");
			struct sensor_type * sen_type = inser_sensor_type(global_config,
					type, mode, wTime);
			if (sen_type == NULL)
				break;
		}
		//[Port*]
		for (i = 1; i <= portNum; i++) {
			sprintf(buffer, "Port%d", i);

			char * portName = get_ini_string(config_doc, buffer, "Name");

			char * portType = get_ini_string(config_doc, buffer, "Type");
			int baudrate = get_ini_int(config_doc, buffer, "Baudrate");
			int sensorType = get_ini_int(config_doc, buffer, "SensorType");

			char* sensorList = get_ini_string(config_doc, buffer, "SensorList");

			struct port_config * pPort = insert_port_config(global_config,
					portName, portType, baudrate, sensorType);

			if (pPort == NULL)
				break;

			if (sensorList != NULL) {
				insert_sensors_list(pPort, sensorList);
			}

		}

	}
	return global_config;

}

static struct sensor_type * inser_sensor_type(struct config_global * gconfig,
		int type, int mode, int time) {
	if (gconfig->sensor_num >= MAX_SENSOR_TYPES)
		return NULL;

	struct sensor_type * sen_type = gconfig->sensor_types
			+ (gconfig->sensor_num);
	sen_type->type = type;
	sen_type->query_mode = mode;
	sen_type->wait_time = time;

	gconfig->sensor_num = gconfig->sensor_num + 1;

	return sen_type;

}
static struct port_config* insert_port_config(struct config_global * gconfig,
		char* name, char* type, int baudrate, int workMode) {

	if (gconfig->portNum >= MAX_PORT_NUM)
		return NULL;

	struct port_config *port = gconfig->ports + (gconfig->portNum);

	port->name = name;
	port->type = type;
	port->baudrate = baudrate;
	port->workMode = workMode;
	port->sensorNum = 0;

	gconfig->portNum = gconfig->portNum + 1;

	return port;

}
