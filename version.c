/*
 * version.c
 *
 *  Created on: Jan 7, 2016
 *      Author: lifeng
 */

#include "version.h"

extern const char const sha1_begin[];
extern const char const sha1_end[];

void get_app_version(struct port_manager * manager) {
	char frame[64];
	int i;
	frame[0] = 0xfe; //ver cmd
	frame[1] = 0; //COM Num

	frame[2] = 1; //1.1.0.0
	frame[3] = 1;
	frame[4] = 0;
	frame[5] = 0;

	for (i = 0; i < sha1_end - sha1_begin; i++) {
		frame[6 + i] = sha1_begin[i];
	}

	send_network_data(manager, frame, 0, 64);
}
