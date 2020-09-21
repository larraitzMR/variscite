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

	int lora_fd;
	int fd_stmf4; //File descriptor for gprs UART
	static const char *device = "/dev/spidev2.0";

	char msg[200];
	char buffRX[50];
	char EPCtoSend[50];

    //Configure SPUI
	fd_stmf4 = openspi(device, 115200);
	if (fd_stmf4 < 0) {
	        printf("Error configuring SPI for variscite\n");
	        fflush(stdout);
	        return (-1);
	}

	lora_fd = create_tcp_conection(5554) ;
	//send_tcp_message("CONECTED");

	int isNodo = 0;

	while(1){
	   	read_tcp_message(msg);
	    if (strncmp(msg, "SET_MODE", 8) == 0) {
			//printf("msg: %s\n", msg);
			char* mens = strtok(msg, " ");
			char* mode = strtok(NULL, " ");
			if(strncmp(mode, "NODO",4) == 0){
				//printf("is NODO\n");
				isNodo = 1;
			}  else {
				//printf("is GATEWAY\n");
				isNodo = 0;
			}
			//transfer(mode, 8, 0, 0);
		} else if (strncmp(msg, "SEND_EPC", 8) == 0){
			printf("msg: %s\n", msg);
			char* mens = strtok(msg, " ");
			char* epc = strtok(NULL, " ");
			sprintf(EPCtoSend, "%s", epc);
			printf("EPCtoSend: %s", EPCtoSend);
		}
		if (isNodo == 0) { //Es GATEWAY, SOLO recibir del ST
			//transfer(0,0,buffRX, sizeof(buffRX));
			//printf("ESTAMOS EN GATEWAY\n");
			transfer("E28011606000020D0EC820DE",24, 0,0);
          	//printf("Buff: %s\n", buffRX);
        	sleep(2);
        	readSPI(0,0, buffRX,24);
			//send_tcp_message("buffRX");
		} else { //Es NODO, enviar por LORA
			printf("ESTAMOS EN NODO\n");
			transfer("E28011606000020D0EC820DE", 24, 0, 0);
			sleep(2);
	        memset(buffRX, 0, sizeof(buffRX));
		}

		/*
		} else if (strncmp(msg, "GATEWAY", 7) == 0) {
			//intercambio de datos entre el ST y variscite
			printf("msg: %s\n", msg);
			char* mens = strtok(msg, " ");
			char* epc = strtok(NULL, " ");
			transfer("E28011606000020D0EC820DE", 24, 0, 0);
			//transfer(epc, 20, 0, 0);
	        sleep(2);
	        readSPI(0,0, buffRX,20);
	        sleep(2);
	        memset(buffRX, 0, sizeof(buffRX));

		} else if (strncmp(msg, "NODO", 4) == 0) {
			//mandarle las EPCs al ST para que este las mande. Sin intercambio de datos
			printf("msg: %s\n", msg);
			char* mens = strtok(msg, " ");
			char* epc = strtok(NULL, " ");
			transfer("E28011606000020D0EC820DE", 24, 0, 0);
//			transfer(epc, 500, 0, 0);
	        sleep(2);
	        memset(buffRX, 0, sizeof(buffRX));

		} else if (strncmp(msg, "DISCONNECT", 10) == 0) {
			close(lora_fd);
		}*/

	}
	fflush(stdout);
	memset(msg, 0, sizeof(msg));

	//mirar senal SIGINT para cerrar los sockets
	//close_tcp_connection();
	closespi();
	return EXIT_SUCCESS;

} // MAIN END
