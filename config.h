/*
 * config.h
 *
 *  Created on: Dec 3, 2013
 *      Author: lifeng
 */

#ifndef CONFIG_H_
#define CONFIG_H_

#define MAX_PORT_NUM (10)
#define MAX_SENSOR_TYPES	(100)
#define MAX_SENSORS_PER_PORT (200)

#include "ini_doc.h"

struct sensor_config
{
	int addr;
	int type;
};

struct port_config
{
	char *name;
	char *type;
	int baudrate;
	int workMode;
	int sensorNum;

	struct sensor_config  sensors[MAX_SENSORS_PER_PORT];

};

struct sensor_type
{
	int type;
	int query_mode;
	int wait_time;
};

struct config_global
{
	struct ini_doc * pDoc;
	int portNum;
	int networkPort;
	int sensor_num;
	struct port_config ports[MAX_PORT_NUM];

	struct sensor_type sensor_types[MAX_SENSOR_TYPES];

};




struct config_global * get_global_config();

struct sensor_type * get_sensor_type(int type);

#endif /* CONFIG_H_ */
