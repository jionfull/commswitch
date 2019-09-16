/*
 * hello.c
 *
 *  Created on: Jan 4, 2016
 *      Author: lifeng
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <libgen.h>

#include "port_manager.h"
int main(int argc, char**argv) {

	//if (daemon(1, 1) == -1) {
	//	exit(-1);
	//	perror("daemon error\r\n");
	//}

	chdir(dirname(argv[0])); //change current dir to application dir

	printf("start running max 64\n");
	struct port_manager* manager = get_port_manager();

	start_port_manager(manager);

	pthread_exit(NULL);

	exit(0);

}

