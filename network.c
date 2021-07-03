/*
 * network.c
 *
 *  Created on: Dec 3, 2013
 *      Author: lifeng
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <netinet/in.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

#include "network.h"
#include "port_manager.h"

#include "version.h"

#define DLE		(0x10)
#define STX		(0x02)
#define ETX 	(0x03)

static void * network_listen(void * arg);
static void * client_proc(void * arg);
static int create_client(struct network* pNetwork, int sock_client);
static void process_frame(char* frame, int length);

static struct network* pgNetwork = NULL;

struct network * create_network(int netPort) {
	if (pgNetwork == NULL) {
		pgNetwork = (struct network*) malloc(sizeof(struct network));
		pgNetwork->port = netPort;
		int i;
		for (i = 0; i < MAX_CLIENT_NUM; i++) {
			pgNetwork->clients[i].used = 0;
		}
		pthread_mutex_init(&(pgNetwork->mutex), NULL);

	}
	return pgNetwork;
}

void start_network(struct network * network) {

	if (network == NULL)
		return;

	pthread_create(&(network->threadListen), NULL, network_listen, network);

}
void stop_network(struct network * network) {

}

void network_send(struct network* network, char* buffer, int offset, int length) {
	if (network == NULL)
		return;

	int i;
	int cnt = 0;
	for (i = 0; i < MAX_CLIENT_NUM; i++) {
		if (network->clients[i].used) {
			cnt = 1;
			break;
		}
	}
	if (cnt == 0)
		return;

	char * tx_buffer = network->tx_buffer;
	int index = 0;

	pthread_mutex_lock(&(network->mutex));

	tx_buffer[0] = DLE;
	tx_buffer[1] = STX;
	index = 2;
	for (i = 0; i < length; i++) {
		tx_buffer[index] = buffer[offset + i];
		index++;
		if (buffer[offset + i] == DLE) {
			tx_buffer[index] = DLE;
			index++;
		}
	}
	tx_buffer[index] = DLE;
	index++;
	tx_buffer[index] = ETX;
	index++;
	for (i = 0; i < MAX_CLIENT_NUM; i++) {
		if (network->clients[i].used) {
			if (send(network->clients[i].socket, tx_buffer, index, 0) < 0) {
				if (errno != EAGAIN) {
					network->clients[i].used = 0;
					close(network->clients[i].socket);
				}

			}
		}
	}

	pthread_mutex_unlock(&(network->mutex));

}

static void * network_listen(void * arg) {
	struct network * pNetwork = (struct network *) arg;
	struct sockaddr_in addr_server;

	int socket_server = socket(AF_INET, SOCK_STREAM, 0);

	pNetwork->socket_server = socket_server;
	if (socket_server < 0) {
		perror("can't create socket");
		exit(1);
	}

	bzero(&addr_server, sizeof(addr_server));

	addr_server.sin_family = AF_INET;
	addr_server.sin_addr.s_addr = htons(INADDR_ANY);
	addr_server.sin_port = htons(pNetwork->port);

	if (bind(socket_server, (struct sockaddr*) &addr_server,
			sizeof(addr_server))) {
		perror("bind failed!");
		exit(1);

	}
	int option = 1;
    setsockopt ( socket_server, SOL_SOCKET, SO_REUSEADDR, &option,sizeof( option ));

	if (listen(socket_server, 20)) {
		perror("listen failed");
		exit(1);
	}

	while (1) {
		struct sockaddr_in client_addr;
		int sock_client;
		int index;
		socklen_t length = sizeof(client_addr);
		sock_client = accept(socket_server, (struct sockaddr*) &client_addr,
				&length);
		if (sock_client < 0) {
			break;
		}

		//set socket non block
		int flags = fcntl(sock_client, F_GETFL, 0);
		fcntl(sock_client, F_SETFL, flags | O_NONBLOCK);

		index = create_client(pNetwork, sock_client);

		if (index < 0) {
			printf("connect client number out of range\n");
			close(sock_client);
		}
		else
		{
			printf("connect client use thread %d\n",index);
		}

	}

	return NULL;
}

static int create_client(struct network* pNetwork, int sock_client) {
	int i;
	int index = -1;
	for (i = 0; i < MAX_CLIENT_NUM; i++) {
		if (pNetwork->clients[i].used == 0) {
			pNetwork->clients[i].used = 1;
			index = i;
			pNetwork->clients[i].socket = sock_client;
			pthread_create(&(pNetwork->clients[i].threadWk), NULL, client_proc,
					&(pNetwork->clients[i]));
			break;
		}
	}
	return index;
}

static void * client_proc(void * arg) {

	struct network_client * pClient = (struct network_client *) arg;

	char* const rx_buffer = pClient->rx_buffer;
	char* const frame_buffer = pClient->frame_buffer;

	int frameLength = 0;
	int metDLE = 0;
	int length;
	int i;
	fd_set rdFds;
	fd_set errFds;
	printf("new connect thread run\n");
	while (1) {

		FD_ZERO(&rdFds);
		FD_ZERO(&errFds);
		FD_SET(pClient->socket, &rdFds);
		FD_SET(pClient->socket, &errFds);
		int ret = select(pClient->socket + 1, &rdFds, NULL, &errFds, NULL);

		if (ret == -1) {
			pClient->used = 0;
			break;
		}

		if (FD_ISSET(pClient->socket, &errFds)) {
			pClient->used = 0;
			break;
		}

		if (FD_ISSET(pClient->socket, &rdFds)) {
			length = recv(pClient->socket, rx_buffer, NET_BUFFER_LENGTH, 0);

			if (length <= 0) {
				pClient->used = 0;
				break;
			}
			printf("recve data %d\n",length );
			for (i = 0; i < length; i++) {

				switch (rx_buffer[i]) {
				case DLE:
					if (frameLength >= 2 && (metDLE == 0)) {
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
							process_frame(frame_buffer, frameLength);
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
				if (frameLength >= NET_FRAME_LENGTH) {
					frameLength = 0;
					metDLE = 0;
				}
			}
		}

	}
	close(pClient->socket);
	printf("one connect thread stop\n");
	return NULL;

}

static void process_frame(char* frame, int length) {

	printf("process cmd %x,%x\n",frame[2],frame[3]);
	struct port_manager * manager = get_port_manager();
	switch (frame[2]) {
	case 1: //CAN data
		to_can_data(manager, frame + 3, length - 5);
		break;
	case 2: //Serial data
		to_serial_data(manager, frame + 3, length - 5);
		break;
	case (char) 0xf2: //set gather work mode
		set_port_mode(manager, frame + 3, length - 5);
		break;
	case (char) 0xf3:
		get_port_mode(manager, 0xf3, frame + 3, length - 5);
		break;
	case (char) 0xfe:
		get_app_version(manager);
		break;

	}

}

