/*
 ============================================================================
 Name        : geo_communications.c
 Author      : xabi1.1
 Version     : 0.1
 Copyright   : Fixso 2017
 Description : This program is intended to run over Variscite Mx6-Dart SOM.
 The purpose is interfacing ThingsMagic M6E RFID module in
 order to read RFID tags in the 868 MHz band. TAGs read must
 be reported to geo_communication module, which will be in
 charge of cloud communications

 Notes           : -lpthread flag required for compiling.
 ============================================================================
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <inttypes.h>
#include "m6e/tm_reader.h"
#include "network.h"
#include "geo_rfid.h"
#include "reader_params.h"

// =======================================================================
//
//  GLOBAL VARIABLES
//
// =======================================================================
//const uint8_t ant_buffer[] = {1, 2, 3, 4};
uint8_t ant_buffer[] = { };
uint8_t ant_count = 0;

int main(void) {
	//uint8_t *antennaList = ant_buffer;
	TMR_Reader r, *rp;
	TMR_Status tmr_ret;
	TMR_Region region;

	void ** dato;
	char *msg;

	char powerMinMax[10];
	char power[2];
	char Ready[5] = "READY";
	int puertos[4] = { NULL };
	int puertosC[4] = { NULL };
	int selAnt[4] = { NULL };
	char puertosConect[4];
	char puertosAnt[4];
	char antenaCheck[1];
	char selecAntenna[4];
	char* infoReader[4];

	//M6E reader instance
	rp = &r;
	tmr_ret = TMR_create(rp, RFID_READER_URI);
	if (tmr_ret != TMR_SUCCESS) {
		printf("geo_rfid: ERROR CREATING READER INSTANCE\n");
		fflush(stdout);
		exit(1);
	}

	//Connect to the reader
	tmr_ret = TMR_connect(rp);
	if (tmr_ret != TMR_SUCCESS) {
		printf("geo_rfid: ERROR CONNECTING M6E\n");
		fflush(stdout);
		exit(1);
	}

	//Commit region
	region = TMR_REGION_NONE;
	tmr_ret = TMR_paramGet(rp, TMR_PARAM_REGION_ID, &region);
	if (tmr_ret != TMR_SUCCESS) {
		printf("geo_rfid: ERROR SETTING REGION\n");
		fflush(stdout);
		exit(1);
	}

	if (TMR_REGION_NONE == region) {
		TMR_RegionList regions;
		TMR_Region _regionStore[32];
		regions.list = _regionStore;
		regions.max = sizeof(_regionStore) / sizeof(_regionStore[0]);
		regions.len = 0;

		tmr_ret = TMR_paramGet(rp, TMR_PARAM_REGION_SUPPORTEDREGIONS, &regions);
		if (tmr_ret != TMR_SUCCESS) {
			printf("geo_rfid: ERROR GETTING SUPPORTED REGION\n");
			fflush(stdout);
			exit(1);
		}

		if (regions.len < 1) {
			if (tmr_ret != TMR_SUCCESS) {
				printf("geo_rfid: Reader doesn't support any regions\n");
				fflush(stdout);
				exit(1);
			}
		}
		region = regions.list[0];
		printf("SUPPORTED REGION: %d", region);
		tmr_ret = TMR_paramSet(rp, TMR_PARAM_REGION_ID, &region);
		if (tmr_ret != TMR_SUCCESS) {
			printf("geo_rfid: ERROR SETTING REGION\n");
			fflush(stdout);
			exit(1);
		}
	}

	tmr_ret = TMR_paramSet(rp, TMR_PARAM_REGION_ID, &region);
	if (tmr_ret != TMR_SUCCESS) {
		printf("geo_rfid: ERROR SETTING SIMPLE READ PLAN\n");
		fflush(stdout);
		exit(1);
	}

	//Read plan: No antenna list provided since M6E supports auto-detect
	TMR_ReadPlan plan;
	tmr_ret = TMR_RP_init_simple(&plan, ant_count, NULL, TMR_TAG_PROTOCOL_GEN2,
			1000);
	if (tmr_ret != TMR_SUCCESS) {
		printf("geo_rfid: ERROR INITIALIZING SIMPLE READ PLAN\n");
		fflush(stdout);
		exit(1);
	}

	//Commit read plan
	tmr_ret = TMR_paramSet(rp, TMR_PARAM_READ_PLAN, &plan);
	if (tmr_ret != TMR_SUCCESS) {
		printf("geo_rfid: ERROR SETTING SIMPLE READ PLAN\n");
		fflush(stdout);
		exit(1);
	}

	//Configure udp socket
	int socket_fd = configure_udp_socket(RFID_PORT);
	if (socket_fd < 0) {
#ifdef RFID_DEBUG
		printf("geo_rfid: ERROR CONFIGURING UDP SOCKET\n");
		printf("%s\n", strerror(errno));
		fflush(stdout);
#endif
		exit(1);
	}

	enviar_udp_msg(socket_fd, Ready, COMMUNICATIONS_PORT);
	while (strcmp(msg, "CONECTADO") != 0) {
		read_udp_message(socket_fd, msg, strlen(msg));
		enviar_udp_msg(socket_fd, Ready, COMMUNICATIONS_PORT);
		//printf("msg: %s", msg);
	}
	//TODO: hace algo mal al guardar los puertos de las antenas, verificar.
	while (1) {
		read_udp_message(socket_fd, msg, strlen(msg));
		printf("msg: %s\n", msg);

		if (strcmp(msg, "POWER_MINMAX") == 0) {
			dato = getParam(rp, TMR_PARAM_RADIO_POWERMAX);
			float max = (uint16_t) dato / 100;
			powerMinMax[0] = max;

			dato = getParam(rp, TMR_PARAM_RADIO_POWERMIN);
			float min = (uint16_t) dato / 100;
			powerMinMax[1] = min;

			enviar_udp_msg(socket_fd, powerMinMax, PARAMS_PORT);
			memset(msg, 0, 50);

		} else if (strcmp(msg, "CON_ANT_PORTS") == 0) {
			getConnectedAntennaPorts(rp, puertosC);

			puertosConect[0] = puertosC[0];
			puertosConect[1] = puertosC[1];
			puertosConect[2] = puertosC[2];
			puertosConect[3] = puertosC[3];

			enviar_udp_msg(socket_fd, puertosConect, PARAMS_PORT);
			memset(msg, 0, 50);
			memset(puertosC, 0, 4);

		} else if (strcmp(msg, "ANT_PORTS") == 0) {
			getAntennaPorts(rp, puertos);

			puertosAnt[0] = puertos[0];
			puertosAnt[1] = puertos[1];
			puertosAnt[2] = puertos[2];
			puertosAnt[3] = puertos[3];

			enviar_udp_msg(socket_fd, puertosAnt, PARAMS_PORT);
			memset(msg, 0, 50);
			memset(puertos, 0, 4);

		} else if (strcmp(msg, "IS_ANT_CHECK_PORT_EN") == 0) {
			dato = getParam(rp, TMR_PARAM_ANTENNA_CHECKPORT);
			antenaCheck[0] = (int) dato;
			enviar_udp_msg(socket_fd, antenaCheck, PARAMS_PORT);
			memset(msg, 0, 50);

			//TODO: despues de esto comprobar que se puede leer con cualquier antena

		} else if (strncmp(msg, "SET_ANT_CHECK_PORT", 18) == 0) {
			char* str = NULL;
			char* busca = "true";
			bool value;

			str = strstr(msg, busca);

			if (str) {
				value = true;
				printf("TRUE");
			} else {
				value = false;
				printf("FALSE");
			}
			tmr_ret = TMR_paramSet(rp, TMR_PARAM_ANTENNA_CHECKPORT, &value);
			//TODO: despues de esto comprobar que se puede leer con cualquier antena
		} else if (strcmp(msg, "GET_POWER") == 0) {
			dato = getParam(rp, TMR_PARAM_RADIO_READPOWER);
			int pow = (uint32_t) dato / 100;
			int dec = (uint32_t) dato % 100;
			printf("Pow: %d, dec: %d\n", pow, dec);
			power[0] = pow;
			power[1] = dec;
			enviar_udp_msg(socket_fd, power, PARAMS_PORT);
			memset(msg, 0, 50);

		} else if (strncmp(msg, "SET_POWER", 9) == 0) {
			int longitud = strlen(msg) - 9;
			char *nuevo = (char*) malloc(sizeof(char) * (longitud + 1));
			nuevo[longitud] = '\0';
			strncpy(nuevo, msg + 9, longitud);
			printf("Set_Power: %s\n", nuevo);
			int32_t value = atoi(nuevo);
			// printf("Atoi: %d\n", value);
			tmr_ret = TMR_paramSet(rp, TMR_PARAM_RADIO_READPOWER, &value);
			tmr_ret = TMR_paramSet(rp, TMR_PARAM_RADIO_WRITEPOWER, &value);

		} else if (strcmp(msg, "GET_SEL_ANT") == 0) {
			getSelectedAntennas(rp, selAnt);
			selecAntenna[0] = selAnt[0];
			selecAntenna[1] = selAnt[1];
			selecAntenna[2] = selAnt[2];
			selecAntenna[3] = selAnt[3];

			enviar_udp_msg(socket_fd, selecAntenna, PARAMS_PORT);
			memset(msg, 0, 50);
			memset(selAnt, 0, 4);
			memset(selecAntenna, 0, 4);

		} else if (strncmp(msg, "SET_SEL_ANT", 11) == 0) {
			TMR_Status ret;
			TMR_ReadPlan plan;
			char *nuevoDato;
			TMR_uint8List listaAntenas = { NULL, 0, 0 };
			ret = TMR_paramGet(rp, TMR_PARAM_READ_PLAN, &plan);
			int longitud = strlen(msg) - 11;
			nuevoDato = (char*) malloc(sizeof(char) * (longitud + 1));
			nuevoDato[longitud] = '\0';
			strncpy(nuevoDato, msg + 11, longitud);
			getAntennaList(nuevoDato, &listaAntenas);
			plan.u.simple.antennas = listaAntenas;

			tmr_ret = TMR_paramSet(rp, TMR_PARAM_READ_PLAN, &plan);
			memset(msg, 0, 50);
		} else if (strcmp(msg, "GET_INFO") == 0) {
			char info[22];

			getReaderInfo(rp, TMR_PARAM_VERSION_MODEL, info);
			printf("InfoReader: %s\n", info);
			enviar_udp_msg(socket_fd, info, PARAMS_PORT);
			memset(info, 0, 50);

			getReaderInfo(rp, TMR_PARAM_VERSION_HARDWARE, info);
			printf("InfoReader: %s\n", info);
			enviar_udp_msg(socket_fd, info, PARAMS_PORT);
			memset(info, 0, 50);

			getReaderInfo(rp, TMR_PARAM_VERSION_SERIAL, info);
			printf("InfoReader: %s\n", info);
			enviar_udp_msg(socket_fd, info, PARAMS_PORT);
			memset(info, 0, 50);

			getReaderInfo(rp, TMR_PARAM_VERSION_SOFTWARE, info);
			printf("InfoReader: %s\n", info);
			enviar_udp_msg(socket_fd, info, PARAMS_PORT);
			memset(info, 0, 50);
		} else if (strcmp(msg, "START_READING") == 0) {

		}
	}

	TMR_destroy(rp);
	return EXIT_SUCCESS;
} // MAIN END


void enviar_udp_msg(int socket_fd, char *data, int port) {
	int errors = 0;
	if (send_udp_msg(socket_fd, "192.168.1.46", port, data, strlen(data)) < 0) {
#ifdef RFID_DEBUG
		printf("geo_rfid: ERROR SENDING UDP MESSAGE\n");
		fflush(stdout);
#endif
		errors++;
	}

}
