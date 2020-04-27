 /*
 ============================================================================
 Name        : main.c
 Author      : Larraitz
 Version     : 1
 Copyright   : MyRuns 2020
 Description : This program is intended to run over Variscite Mx6-Dart SOM.
 Notes		 : -lpthread flag required for compiling.
 ============================================================================
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <time.h>
#include <linux/spi/spidev.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/time.h>

#include "spi.h"
#include "lmic/lmic.h"
#include "sim808.h"
#include "network.h"
#include "geo_rfid.h"
#include "main.h"


// =======================================================================
//
//  GLOBAL VARIABLES
//
// =======================================================================

int main(void) {

	int gprs_fd;

	int n = 0;
	char msg[50];

	
	gprs_fd = create_tcp_conection(5554) ;
	//send_tcp_message("Mensaje TCP");
	

	while(1){
	   	read_tcp_message(msg);
	   //	printf("msg: %s\n", msg);
	    if (strncmp(msg, "GET_NETWORK", 11) == 0) {
			printf("msg: %s\n", msg);
			int longitud = strlen(msg) - 11;
			char *nuevo = (char*) malloc(sizeof(char) * (longitud + 1));
			nuevo[longitud] = '\0';
			strncpy(nuevo, msg + 11, longitud);
		}
		else if (strncmp(msg, "SET_NETWORK", 11) == 0) {
			printf("msg: %s\n", msg);
			int longitud = strlen(msg) - 11;
			char *nuevo = (char*) malloc(sizeof(char) * (longitud + 1));
			nuevo[longitud] = '\0';
			strncpy(nuevo, msg + 11, longitud);
		}
		else if (strncmp(msg, "SEND_SMS", 8) == 0) {
			printf("msg: %s\n", msg);
			int longitud = strlen(msg) - 11;
			char *nuevo = (char*) malloc(sizeof(char) * (longitud + 1));
			nuevo[longitud] = '\0';
			strncpy(nuevo, msg + 11, longitud);
		}
	}
	fflush(stdout);

	return EXIT_SUCCESS;

} // MAIN END
