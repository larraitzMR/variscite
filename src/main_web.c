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
	char* network;
	char net[20];

	//Init GPRS
	int fd_sim808 = 0;
	int result = 0;
	fd_set rfds;
	struct timeval tv;

	printf("Probando GPRS: \n");
	printf("============== \n");
	fflush(stdout);

	//Iniciamos gprs
	fd_sim808 = gprs_init();
	if (fd_sim808 <= 0) {
		printf("Error arrancando gprs\n");
		fflush(stdout);
		return (-1);
	}
	printf("fd_sim808: %d\n", fd_sim808);
	fflush(stdout);
	
	gprs_fd = create_tcp_conection(5554) ;

	//send_tcp_message("CONNECTED");
	

	while(1){
	   	read_tcp_message(msg);
	    if (strncmp(msg, "GET_NETWORK", 11) == 0) {
			printf("msg: %s\n", msg);
			//send_tcp_message("Mensaje TCP");
			gprs_get_network();
		}
		else if (strncmp(msg, "SET_NETWORK", 11) == 0) {
			printf("msg: %s\n", msg);
			int longitud = strlen(msg) - 11;
			char *nuevo = (char*) malloc(sizeof(char) * (longitud + 1));
			nuevo[longitud] = '\0';
			strncpy(nuevo, msg + 12, longitud);
			printf(" : %s\n", nuevo);
			gprs_set_network(nuevo);

		}
		else if (strncmp(msg, "SEND_SMS", 8) == 0) {

			printf("msg: %s\n", msg);
			char* mens = strtok(msg, " ");
			/*int longitud = strlen(msg) - 8;
			char *nuevo = (char*) malloc(sizeof(char) * (longitud + 1));
			nuevo[longitud] = '\0';
			strncpy(nuevo, msg + 8, longitud);
			printf(" : %s\n", nuevo);*/
			char* telf = strtok(NULL, " ");	
			char* text = strtok(NULL, " ");
			gprs_send_SMS(telf, text);
		} else if (strncmp(msg, "DISCONNECT", 10) == 0) {
			gprs_close();
			close(gprs_fd);
		}

	}
	fflush(stdout);

	return EXIT_SUCCESS;

} // MAIN END
