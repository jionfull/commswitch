/*
 * network.h
 *
 *  Created on: Dec 3, 2013
 *      Author: lifeng
 */

#ifndef NETWORK_H_
#define NETWORK_H_

#include <pthread.h>

#define MAX_CLIENT_NUM (5)
#define NET_FRAME_LENGTH	(64*1024)
#define NET_BUFFER_LENGTH	(2*1024)
struct network_client
{
	int used;
	int socket;
	pthread_t threadWk;
	char rx_buffer[NET_BUFFER_LENGTH];
	char frame_buffer[NET_FRAME_LENGTH];

};

struct network
{
	int socket_server;
	int port;
	pthread_t threadListen;
	int client_num;
	struct network_client clients[MAX_CLIENT_NUM];

	char tx_buffer[NET_FRAME_LENGTH];

	pthread_mutex_t mutex;

};







struct network * create_network(int netPort);


void start_network(struct network * network);
void stop_network(struct network * network);

void network_send(struct network* network,char* buffer,int offset,int length);




#endif /* NETWORK_H_ */
