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
	for (i = 0; i < 64; i++) {
		frame[i] = 0;
	}
	frame[0] = 0xfe; //ver cmd
	frame[1] = 0; //COM Num

	frame[10] = 1; //1.1.0.0
	frame[11] = 1;
	frame[12] = 0;
	frame[13] = 0;

	for (i = 0; i < sha1_end - sha1_begin; i++) {
		frame[14 + i] = sha1_begin[i];
	}

	send_network_data(manager, frame, 0, 64);
}
