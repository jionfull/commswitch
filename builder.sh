#!/bin/sh
arm-linux-gnueabihf-gcc -o commswitch can_port.c config.c  frame_manager.c gather_port.c ini_doc.c libsocketcan.c main.c network.c port_manager.c version.c  resource.S -lpthread


