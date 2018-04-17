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
 Notes		 : -lpthread flag required for compiling.
 ============================================================================
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include "m6e/tm_reader.h"
#include "network.h"
#include "geo_rfid.h"
#include "reader_params.h"

// =======================================================================
//
//  GLOBAL VARIABLES
//
// =======================================================================
uint8_t ant_buffer[] = { };
uint8_t ant_count = 0;

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
	}

	//TODO: en la funcion send_udp_msg esta quitado el checksum para que vaya bien

	while (strcmp(msg, "DISCONNECT") != 0) {

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
			bzero(powerMinMax, sizeof(powerMinMax));

		} else if (strcmp(msg, "CON_ANT_PORTS") == 0) {
			getConnectedAntennaPorts(rp, puertosC);

			puertosConect[0] = puertosC[0];
			puertosConect[1] = puertosC[1];
			puertosConect[2] = puertosC[2];
			puertosConect[3] = puertosC[3];

			enviar_udp_msg(socket_fd, puertosConect, PARAMS_PORT);
			bzero(puertosConect, sizeof(puertosConect));
			bzero(puertosC, sizeof(puertosC));

		} else if (strcmp(msg, "ANT_PORTS") == 0) {
			getAntennaPorts(rp, puertos);

			puertosAnt[0] = puertos[0];
			puertosAnt[1] = puertos[1];
			puertosAnt[2] = puertos[2];
			puertosAnt[3] = puertos[3];

			enviar_udp_msg(socket_fd, puertosAnt, PARAMS_PORT);
			bzero(puertosAnt, sizeof(puertosAnt));
			bzero(puertos, sizeof(puertos));

		} else if (strcmp(msg, "IS_ANT_CHECK_PORT_EN") == 0) {
			dato = getParam(rp, TMR_PARAM_ANTENNA_CHECKPORT);
			antenaCheck[0] = (int) dato;
			enviar_udp_msg(socket_fd, antenaCheck, PARAMS_PORT);
			bzero(antenaCheck, sizeof(antenaCheck));

			//TODO: despues de esto comprobar que se puede leer con cualquier antena

		} else if (strncmp(msg, "SET_ANT_CHECK_PORT", 18) == 0) {
			char* str = NULL;
			char* busca = "true";
			bool value;

			str = strstr(msg, busca);

			if (str) {
				value = true;
				printf("TRUE");
				fflush(stdout);
			} else {
				value = false;
				printf("FALSE");
				fflush(stdout);
			}
			tmr_ret = TMR_paramSet(rp, TMR_PARAM_ANTENNA_CHECKPORT, &value);
			//TODO: despues de esto comprobar que se puede leer con cualquier antena

		} else if (strcmp(msg, "GET_POWER") == 0) {
			dato = getParam(rp, TMR_PARAM_RADIO_READPOWER);
			int pow = (uint32_t) dato / 100;
			int dec = (uint32_t) dato % 100;
			printf("Pow: %d, dec: %d\n", pow, dec);
			fflush(stdout);
			power[0] = pow;
			power[1] = dec;
			enviar_udp_msg(socket_fd, power, PARAMS_PORT);
			bzero(power, sizeof(power));

		} else if (strncmp(msg, "SET_POWER", 9) == 0) {
			int longitud = strlen(msg) - 9;
			char *nuevo = (char*) malloc(sizeof(char) * (longitud + 1));
			nuevo[longitud] = '\0';
			strncpy(nuevo, msg + 9, longitud);
			printf("Set_Power: %s\n", nuevo);
			fflush(stdout);
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
			bzero(selecAntenna, sizeof(selecAntenna));
			bzero(selAnt, sizeof(selAnt));

		} else if (strncmp(msg, "SET_SEL_ANT", 11) == 0) {
			TMR_ReadPlan plan;
			char *nuevoDato;
			TMR_uint8List listaAntenas = { NULL, 0, 0 };
			tmr_ret = TMR_paramGet(rp, TMR_PARAM_READ_PLAN, &plan);
			int longitud = strlen(msg) - 11;
			nuevoDato = (char*) malloc(sizeof(char) * (longitud + 1));
			nuevoDato[longitud] = '\0';
			strncpy(nuevoDato, msg + 11, longitud);
			getAntennaList(nuevoDato, &listaAntenas);
			plan.u.simple.antennas = listaAntenas;

			tmr_ret = TMR_paramSet(rp, TMR_PARAM_READ_PLAN, &plan);

		} else if (strcmp(msg, "GET_INFO") == 0) {
			char info[22];

			getReaderInfo(rp, TMR_PARAM_VERSION_MODEL, info);
			printf("InfoReader: %s\n", info);
			fflush(stdout);
			enviar_udp_msg(socket_fd, info, PARAMS_PORT);
			bzero(info, sizeof(info));

			getReaderInfo(rp, TMR_PARAM_VERSION_HARDWARE, info);
			printf("InfoReader: %s\n", info);
			fflush(stdout);
			enviar_udp_msg(socket_fd, info, PARAMS_PORT);
			bzero(info, sizeof(info));

			getReaderInfo(rp, TMR_PARAM_VERSION_SERIAL, info);
			printf("InfoReader: %s\n", info);
			fflush(stdout);
			enviar_udp_msg(socket_fd, info, PARAMS_PORT);
			bzero(info, sizeof(info));

			getReaderInfo(rp, TMR_PARAM_VERSION_SOFTWARE, info);
			printf("InfoReader: %s\n", info);
			fflush(stdout);
			enviar_udp_msg(socket_fd, info, PARAMS_PORT);
			bzero(info, sizeof(info));
		} else if (strcmp(msg, "GET_ADV_OPT") == 0) {

			dato = getParam(rp, TMR_PARAM_REGION_ID);
			printf("REGION: %s\n", dato);
			fflush(stdout);
			enviar_udp_msg(socket_fd, (char *) dato, PARAMS_PORT);

			dato = getParam(rp, TMR_PARAM_GEN2_TARI);
			printf("TARI: %s\n", dato);
			fflush(stdout);
			enviar_udp_msg(socket_fd, (char *) dato, PARAMS_PORT);

			dato = getParam(rp, TMR_PARAM_GEN2_BLF);
			printf("BLF: %s\n", dato);
			fflush(stdout);
			enviar_udp_msg(socket_fd, (char *) dato, PARAMS_PORT);

			dato = getParam(rp, TMR_PARAM_GEN2_TAGENCODING);
			printf("ENCONDING: %s\n", dato);
			fflush(stdout);
			enviar_udp_msg(socket_fd, (char *) dato, PARAMS_PORT);

			dato = getParam(rp, TMR_PARAM_GEN2_Q);
			printf("Q: %s\n", dato);
			fflush(stdout);
			enviar_udp_msg(socket_fd, (char *) dato, PARAMS_PORT);

			dato = getParam(rp, TMR_PARAM_GEN2_SESSION);
			printf("SESION: %s\n", dato);
			fflush(stdout);
			enviar_udp_msg(socket_fd, (char *) dato, PARAMS_PORT);

			dato = getParam(rp, TMR_PARAM_GEN2_TARGET);
			printf("TARGET: %s\n", dato);
			fflush(stdout);
			enviar_udp_msg(socket_fd, (char *) dato, PARAMS_PORT);

		} else if (strncmp(msg, "SET_REGION",10) == 0) {
			int longitud = strlen(msg) - 11;
			char *nuevo = (char*) malloc(sizeof(char) * (longitud + 1));
			nuevo[longitud] = '\0';
			strncpy(nuevo, msg + 11, longitud);
			printf("REGION: %s\n", nuevo);
			fflush(stdout);
			int value = getRegionNumber(nuevo);
			tmr_ret = TMR_paramSet(rp, TMR_PARAM_REGION_ID, &value);

		} else if (strcmp(msg, "GET_SUPPORTED_REGIONS") == 0) {
			getRegionNames(rp, reg);
			for(int i = 0; i<20; i++) {
				regiones[i] = reg[i];
				printf("Reg Nums: %d", reg[i]);
			}
			enviar_udp_msg(socket_fd, regiones, PARAMS_PORT);
			bzero(regiones, sizeof(regiones));

		} else if (strcmp(msg, "SET_TARI") == 0) {

		} else if (strcmp(msg, "SET_BLF") == 0) {

		} else if (strcmp(msg, "SET_M") == 0) {

		} else if (strcmp(msg, "SET_Q") == 0) {

		} else if (strncmp(msg, "SET_SESSION", 11) == 0) {
			int longitud = strlen(msg) - 12;
			char *nuevo = (char*) malloc(sizeof(char) * (longitud + 1));
			nuevo[longitud] = '\0';
			strncpy(nuevo, msg + 12, longitud);
			printf("SESION: %s\n", nuevo);
			fflush(stdout);
			int32_t value = atoi(nuevo);
			// printf("Atoi: %d\n", value);
			tmr_ret = TMR_paramSet(rp, TMR_PARAM_GEN2_SESSION, &value);

		} else if (strncmp(msg, "SET_TARGET", 10) == 0) {
			int longitud = strlen(msg) - 11;
			char *nuevo = (char*) malloc(sizeof(char) * (longitud + 1));
			nuevo[longitud] = '\0';
			strncpy(nuevo, msg + 11, longitud);
			printf("TARGET: %s\n", nuevo);
			fflush(stdout);
			int32_t value;
			if (strcmp(nuevo, "A") == 0) {
				value = 0;
			} else if (strcmp(nuevo, "B") == 0) {
				value = 1;
			} else if (strcmp(nuevo, "AB") == 0) {
				value = 2;
			}
			printf("Value: %d\n", value);
			fflush(stdout);
			tmr_ret = TMR_paramSet(rp, TMR_PARAM_GEN2_TARGET, &value);
		} else if (strcmp(msg, "START_READING") == 0) {
			while (strcmp(msg, "STOP_READING") != 0) {
				//Init tag count
				tag_read_count = 0;

				//Read tags
				tmr_ret	= TMR_readIntoArray(rp, 500, &tag_read_count, &tagReads);
				if (tmr_ret != TMR_SUCCESS) {
					printf("geo_rfid: ERROR READING TAGS INTO ARRAY");
					fflush(stdout);
					errors++;
				} else {
					for (i = 0; i < tag_read_count; i++) {
						TMR_TagReadData* trd;
						char epcStr[128];

						trd = &tagReads[i];
						TMR_bytesToHex(trd->tag.epc, trd->tag.epcByteCount,	epcStr);
						printf("EPC:%s ant:%d count:%u\n", epcStr, trd->antenna, trd->readCount);
						printf("RSSI:%d\n", trd->rssi);
						fflush(stdout);
						//Send antena and epc to geo_communications
						// MESSAGE FORMAT:
						// * | MSG_LEN(1b) | CO(1b) | ANTENNA(1B) | EPC(NB) | CHECKSUM(1b)
						// CHECKSUM is calculated by send_udp_msg() function
						bzero(rfid_report_msg, sizeof(rfid_report_msg));
						rfid_report_msg[0] = trd->antenna;
						rfid_report_msg[1] = trd->rssi;
						int j = 0;
						for (j = 0; j < strlen(epcStr); j++) {
							rfid_report_msg[j+2] = epcStr[j];
						}

						send_udp_msg(socket_fd, "192.168.1.51",RFID_PORT, rfid_report_msg,strlen(epcStr)+2);
					}
				}

				//Check error count
				if (errors > MAX_ERRORS) {
#ifdef RFID_DEBUG
					printf("MAX_ERRORS REACHED. CLOSING\n");
					fflush(stdout);
#endif
					break;
				}
				read_udp_message(socket_fd, msg, strlen(msg));
			}
		}
	}

	TMR_destroy(rp);
	return EXIT_SUCCESS;
} // MAIN END
