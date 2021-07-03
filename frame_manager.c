/*
 * frame_manager.c
 *
 *  Created on: Dec 2, 2013
 *      Author: lifeng
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/time.h>

#include "frame_manager.h"
#include "gather_port.h"
#include "port_manager.h"
#define MAX_MANAGER_NUM (5)
static struct frame_manager manager_array[MAX_MANAGER_NUM];

static void * serial_rx(void * data);
static void * serial_rxlon(void * data);
static void check_frame(struct frame_manager * manager, char* frame, int length);
static void put_frame(struct frame_manager * manager, char* frame, int length);

struct frame_manager * create_frame_manager(int serial_fd) {

	int i;
	struct frame_manager* manager = NULL;
	for (i = 0; i < MAX_MANAGER_NUM; i++) {
		if (manager_array[i].used == 0) {
			manager_array[i].used = 1;
			manager_array[i].frame_count = 0;
			manager_array[i].wrIndex = 0;
			manager_array[i].serial_fd = serial_fd;
			pthread_mutex_init(&manager_array[i].mutext, NULL);
			pthread_cond_init(&manager_array[i].cond, NULL);
			manager = manager_array + i;
			break;
		}
	}
	return manager;
}

void start_frame_manager(struct frame_manager * manager) {

	pthread_t * thread = &(manager->thread_rx);
	if (manager->port->work_mode == MODE_WORK) {
		pthread_create(thread, NULL, serial_rxlon, manager);
	} else {
		pthread_create(thread, NULL, serial_rx, manager);
	}

}

void stop_frame_manager(struct frame_manager * manager) {

}

static void set_rx_time(struct frame_manager* manager) {
	pthread_mutex_lock(&(manager->mutext));
	gettimeofday(&(manager->last_rx_time), NULL);
	pthread_mutex_unlock(&(manager->mutext));
}

/*
 *
 * */
int serial_rx_timeout(struct frame_manager* manager) {

	struct timeval now;
	int interval = 0;
	gettimeofday(&now, NULL);
	pthread_mutex_lock(&(manager->mutext));

	interval = (now.tv_sec - manager->last_rx_time.tv_sec) * 1000;
	interval += (now.tv_usec - manager->last_rx_time.tv_usec) / 1000;

	pthread_mutex_unlock(&(manager->mutext));
	return interval;
}

static void * serial_rxlon(void * data) {
	struct frame_manager* manager = (struct frame_manager*) data;
	char* rx_buffer = manager->rx_buffer;
	char* frame_buffer = manager->frame_buffer;
	int serial_fd = manager->serial_fd;
	int i;
	int metDLE = 0;
	int frameLength = 0;

	set_rx_time(manager); //initialize rx time

	while (1) {
		usleep(10*1000);
		int length = read(serial_fd, rx_buffer, RX_BUFFER_SIZE);

		if (length > 0) {
			set_rx_time(manager);
		}
		else
		{
			frameLength=0;
			metDLE=0;
		}

		for (i = 0; i < length; i++) {

			switch (rx_buffer[i]) {
			case DLE:
				if (metDLE == 0) {
					frame_buffer[frameLength] = DLE;
					frameLength++;
				}
				metDLE = 1 - metDLE;
				break;
			case STX:
				if (metDLE) {
					frame_buffer[0] = DLE;
					frame_buffer[1] = STX;
					frameLength = 2;
				} else if (frameLength >= 2) {
					frame_buffer[frameLength] = STX;
					frameLength++;
				}
				metDLE = 0;
				break;
			case ETX:
				if (frameLength >= 2) {
					frame_buffer[frameLength] = ETX;
					frameLength++;
					if (metDLE) //got a new frame
					{
						check_frame(manager, frame_buffer, frameLength);
						frameLength = 0;
					}
				}
				metDLE = 0;
				break;
			default:
				if (frameLength >= 2) {
					frame_buffer[frameLength] = rx_buffer[i];
					frameLength++;
				}
				metDLE = 0;
				break;
			}
			if (frameLength >= MAX_RX_FRAME_LENGTH) {
				frameLength = 0;
				metDLE = 0;
			}
		}

	}

	return NULL;
}

static void * serial_rx(void * data) {
	struct frame_manager* manager = (struct frame_manager*) data;
	char* rx_buffer = manager->rx_buffer;

	int serial_fd = manager->serial_fd;

	set_rx_time(manager); //initialize rx time

	while (1) {
		usleep(10*1000);
		int length = read(serial_fd, rx_buffer, RX_BUFFER_SIZE);

		if (length > 0) {
			set_rx_time(manager);
			put_frame(manager, rx_buffer, length);
		}

	}

	return NULL;
}

static void check_frame(struct frame_manager * manager, char* frame, int length) {

	int len = frame[2] + (frame[3] << 8);

	if (len != length - 6)
		return;

	put_frame(manager, frame, length);

}

extern void trigger_rx(struct gather_port *port);
static void put_frame(struct frame_manager * manager, char* frame, int length) {

	if ((manager->port->work_mode) == MODE_DEBUG) //调试模式透传
			{
		struct gather_port *port = manager->port;
		char *rx_frame = port->rx_data;
		int i;
		struct port_manager * portManager = get_port_manager();
		rx_frame[0] = 0x02; //Serial-data
		rx_frame[1] = port->portIndex; //COM Num

		rx_frame[2] = 0;
		rx_frame[3] = 0;
		rx_frame[4] = 0;
		rx_frame[5] = 0;

		rx_frame[6] = 0;
		rx_frame[7] = 0;
		rx_frame[8] = 0;
		rx_frame[9] = 0;

		rx_frame[10] = 0xff; //sensorType
		for (i = 0; i < length - 4; i++) {
			rx_frame[11 + i] = frame[i + 2];
		}
		trigger_rx(port);
		send_network_data(portManager, rx_frame, 0, length - 4 + 11);
	} else if ((manager->port->work_mode) == MODE_MODBUS) //工作模式
			{
		struct gather_port *port = manager->port;
		char *rx_frame = port->rx_data;
		int i;
		struct port_manager * portManager = get_port_manager();
		rx_frame[0] = 0x02; //Serial-data
		rx_frame[1] = port->portIndex; //COM Num

		rx_frame[10] = 0xff; //sensorType
		for (i = 0; i < length; i++) {
			rx_frame[2 + i] = frame[i];
		}
		trigger_rx(port);
		send_network_data(portManager, rx_frame, 0, length + 2);
	} else if ((manager->port->work_mode) == MODE_DLT645) //工作模式
			{
		struct gather_port *port = manager->port;
		char *rx_frame = port->rx_data;
		int i;
		struct port_manager * portManager = get_port_manager();
		rx_frame[0] = 0x02; //Serial-data
		rx_frame[1] = port->portIndex; //COM Num

		rx_frame[10] = 0xff; //sensorType
		for (i = 0; i < length; i++) {
			rx_frame[2 + i] = frame[i];
		}
		trigger_rx(port);
		send_network_data(portManager, rx_frame, 0, length + 2);
	} else //工作模式
	{
		pthread_mutex_t * pMutex = &(manager->mutext);
		pthread_mutex_lock(pMutex);

		//头2字节代表祯长度
		int wrIndex = manager->wrIndex;
		int cnt = manager->frame_count;

		char * buffer = manager->frame_array[wrIndex];

		length -= 4;

		buffer[0] = length & 0xff;
		buffer[1] = length >> 8;

		int i;
		for (i = 0; i < length; i++) {
			buffer[2 + i] = frame[2 + i];
		}

		if (cnt < MAX_FRAME_COUNT) {
			cnt++;
			manager->frame_count = cnt;
		}
		wrIndex++;
		if (wrIndex >= MAX_FRAME_COUNT) {
			wrIndex = 0;
		}
		manager->wrIndex = wrIndex;

		pthread_cond_signal(&(manager->cond));

		pthread_mutex_unlock(pMutex);
	}

}

void clear_all_frame(struct frame_manager * manager) {
	pthread_mutex_t * pMutex = &(manager->mutext);
	pthread_mutex_lock(pMutex);

	manager->wrIndex = 0;
	manager->frame_count = 0;

	pthread_mutex_unlock(pMutex);
}
void get_frame(struct frame_manager * manager, char * buffer, int *length,
		int timeout_ms) {

	pthread_mutex_t * pMutex = &(manager->mutext);
	pthread_cond_t * pCond = &(manager->cond);
	*length = 0;
	pthread_mutex_lock(pMutex);
	int i;
	int cnt = manager->frame_count;

	if (cnt > 0) { //读取数据

		int wrIndex = manager->wrIndex;

		int rdIndex = (wrIndex - cnt + MAX_FRAME_COUNT) % MAX_FRAME_COUNT;
		char * src = manager->frame_array[rdIndex];
		int len = src[0] + (src[1] << 8);

		for (i = 0; i < len; i++) {
			buffer[i] = src[2 + i];
		}
		*length = len;

		manager->frame_count = cnt - 1;
	} else {

		struct timespec abstime;
		struct timeval now;
		gettimeofday(&now, NULL);
		int nsec = now.tv_usec * 1000 + (timeout_ms % 1000) * 1000000;
		abstime.tv_nsec = nsec % 1000000000;
		abstime.tv_sec = now.tv_sec + nsec / 1000000000 + timeout_ms / 1000;

		pthread_cond_timedwait(pCond, pMutex, &abstime);
		cnt = manager->frame_count;
		if (cnt > 0) {
			int wrIndex = manager->wrIndex;

			int rdIndex = (wrIndex - cnt + MAX_FRAME_COUNT) % MAX_FRAME_COUNT;
			char * src = manager->frame_array[rdIndex];
			int len = src[0] + (src[1] << 8);

			for (i = 0; i < len; i++) {
				buffer[i] = src[2 + i];
			}
			*length = len;

			manager->frame_count = cnt - 1;
		}

	}

	pthread_mutex_unlock(pMutex);

}
void send2sensor(struct frame_manager *manager, char* frame, int length) {
	int data_len = length + 2;
	char d1 = (char) (data_len & 0xff);
	char d2 = (char) ((data_len >> 8) & 0xff);

	unsigned short sum = (d1 + d2);
	int i;

	for (i = 0; i < length; i++) {
		sum += frame[i];
	}
	char c1 = (char) (sum & 0xff);
	char c2 = (char) ((sum >> 8) & 0xff);

	char * buffer = manager->tx_buffer;

	int index = 0;

	buffer[0] = DLE;
	buffer[1] = STX;
	index = 2;
	//data length
	buffer[index] = d1;
	index++;
	if (d1 == DLE) {
		buffer[index] = DLE;
		index++;
	}
	buffer[index] = d2;
	index++;
	if (d2 == DLE) {
		buffer[index] = DLE;
		index++;
	}
	// real data
	for (i = 0; i < length; i++) {
		buffer[index] = frame[i];
		index++;
		if (frame[i] == DLE) {
			buffer[index] = DLE;
			index++;
		}

	}
	//check sum
	buffer[index] = c1;
	index++;
	if (c1 == DLE) {
		buffer[index] = DLE;
		index++;
	}
	buffer[index] = c2;
	index++;
	if (c2 == DLE) {
		buffer[index] = DLE;
		index++;
	}
	buffer[index] = DLE;
	index++;
	buffer[index] = ETX;
	index++;

	write(manager->serial_fd, buffer, index);

}
void frame_manager__send_raw(struct frame_manager *manager, char* frame,
		int length) {

	write(manager->serial_fd, frame, length);

}
