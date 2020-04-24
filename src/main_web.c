 /*
 ============================================================================
 Name        : geo_communications.c
 Author      : xabi1.1
 Version     : 0.1
 Copyright   : MyRuns 2017
 Description : This program is intended to run over Variscite Mx6-Dart SOM.
 The purpose is interfacing ThingsMagic M6E RFID module in
 order to read RFID tags in the 868 MHz band. TAGs read must
 be reported to geo_communication module, which will be in
 charge of cloud communications
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
#include "spi.h"
#include "lmic/lmic.h"
#include "sim808.h"
#include "m6e/tm_reader.h"
#include "network.h"
#include "geo_rfid.h"
#include "reader_params.h"
#include "sqlite3.h"
#include "main.h"

// =======================================================================
//
//  GLOBAL VARIABLES
//
// =======================================================================
uint8_t ant_buffer[] = { };
uint8_t ant_count = 0;
struct tablaEPC tabla[300];
sqlite3 *db;
int numeroEPCs;

// log text
static void initfunc(osjob_t* job) {
	// reschedule job every second
	os_setTimedCallback(job, os_getTime() + sec2osticks(1), initfunc);
}

static int callback(void *data, int argc, char **argv, char **azColName) {
	int i;
	fprintf(stderr, "%s: ", (const char*) data);

	for (i = 0; i < argc; i++) {
		printf("%s = %s\n", azColName[i], argv[i] ? argv[i] : "NULL");
	}

	printf("\n");
	return 0;
}

int main(void) {

	TMR_Reader r, *rp;
	TMR_Status tmr_ret;
	TMR_Region region;

	int32_t tag_read_count; //Counter for number of read tags
	TMR_TagReadData* tagReads;
	int i;
	int errors = 0;
	char rfid_report_msg[64];

	void ** dato;
	char msg[25];

	char powerMinMax[10];
	char power[2];
	char Ready[5] = "READY";
	int reg[20];
	int puertos[4];
	int puertosC[4];
	int selAnt[4];
	char puertosConect[4];
	char puertosAnt[4];
	char antenaCheck[1];
	char selecAntenna[4];
	char regiones[20];

	int rc;
	char *zErrMsg = 0;
	char *sql;

	//Configure udp socket
	int socket_fd = configure_udp_socket(WEB_PORT);
	if (socket_fd < 0) {
#ifdef RFID_DEBUG
		printf("ERROR CONFIGURING UDP SOCKET\n");
		printf("%s\n", strerror(errno));
		fflush(stdout);
#endif
		exit(1);
	}

	enviar_udp_msg(socket_fd, Ready, WEB_PORT);
	while (strncmp(msg, "CONECTADO", 9) != 0) {
//		read_udp_message(socket_fd, msg, strlen(msg));
//		printf("msg: %s\n", msg);
		enviar_udp_msg(socket_fd, Ready, WEB_PORT);
		printf("Enviado: %s\n", Ready);
	}


	while (strcmp(msg, "DISCONNECT") != 0) {

		read_udp_message(socket_fd, msg, strlen(msg));
		if (strncmp(msg, "GET_NETWORK", 7) == 0) {
			printf("msg: %s\n", msg);
			int longitud = strlen(msg) - 11;
			char *nuevo = (char*) malloc(sizeof(char) * (longitud + 1));
			nuevo[longitud] = '\0';
			strncpy(nuevo, msg + 11, longitud);
		}
		else if (strncmp(msg, "SET_NETWORK", 7) == 0) {
			printf("msg: %s\n", msg);
			int longitud = strlen(msg) - 11;
			char *nuevo = (char*) malloc(sizeof(char) * (longitud + 1));
			nuevo[longitud] = '\0';
			strncpy(nuevo, msg + 11, longitud);
		}
		else if (strncmp(msg, "SEND_SMS", 15) == 0) {
			printf("msg: %s\n", msg);
			int longitud = strlen(msg) - 11;
			char *nuevo = (char*) malloc(sizeof(char) * (longitud + 1));
			nuevo[longitud] = '\0';
			strncpy(nuevo, msg + 11, longitud);
		}
	}

	TMR_destroy(rp);
	return EXIT_SUCCESS;
} // MAIN END
