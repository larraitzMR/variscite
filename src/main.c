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

	tabla->numEPC = 0;

	/* Open database */
	rc = sqlite3_open("test.db", &db);

	if (rc) {
		fprintf(stderr, "Can't open database: %s\n", sqlite3_errmsg(db));
		return (0);
	} else {
		fprintf(stderr, "Opened database successfully\n");
	}

//	sql = "CREATE TABLE IF NOT EXISTS INVENTORY ("
//			"ID INTEGER PRIMARY KEY, "
//			"TIME INTEGER, "
//			"EPC TEXT, "
//			"TID TEXT, "
//			"RSSI INTEGER, "
//			"READER_SLOT INTEGER);";

	sql = "CREATE TABLE IF NOT EXISTS PRUEBAEPC ("
			"ID INTEGER PRIMARY KEY AUTOINCREMENT,"
			"EPC TEXT,"
			"HORA TEXT,"
			"ENVIADO INTEGER);";

	/* Execute SQL statement */
	rc = sqlite3_exec(db, sql, callback, 0, &zErrMsg);

	if (rc != SQLITE_OK) {
		fprintf(stderr, "SQL error: %s\n", zErrMsg);
		sqlite3_free(zErrMsg);
	} else {
		fprintf(stdout, "Success\n");
	}

//	//Init GPRS
//	int fd_sim808 = 0;
//	int result = 0;
//	fd_set rfds;
//	struct timeval tv;
//
//	printf("Probando GPRS: \n");
//	printf("============== \n");
//	fflush(stdout);
//
//	//Iniciamos gprs
//	fd_sim808 = gprs_init();
//	if (fd_sim808 <= 0) {
//		printf("Error arrancando gprs\n");
//		fflush(stdout);
//		return (-1);
//	}
//	printf("fd_sim808: %d\n", fd_sim808);
//	fflush(stdout);
//
//	//Tiempos
//	time_t program_start = time(NULL);
//	time_t last_send_position = time(NULL);
//
//	///LORA library initialization
//	osjob_t initjob;
//
//	// initialize runtime env
//	os_init();
//
//	// setup initial job
//	os_setCallback(&initjob, initfunc);

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
		printf("geo_rfid: Error connecting reader: %s\n",
				TMR_strerr(rp, tmr_ret));
		fflush(stdout);
		exit(1);
	}

	//Commit region
	region = TMR_REGION_NONE;
	tmr_ret = TMR_paramGet(rp, TMR_PARAM_REGION_ID, &region);
	//printf("geo_rfid: Region: %s\n", TMR_strerr(rp, tmr_ret));
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
				printf("geo_rfid: Reader doesn't support any region\n");
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

//	enviar_udp_msg(socket_fd, Ready, COMMUNICATIONS_PORT);
//	while (strncmp(msg, "CONECTADO", 9) != 0) {
//		read_udp_message(socket_fd, msg, strlen(msg));
//		printf("msg: %s\n", msg);
//		enviar_udp_msg(socket_fd, Ready, COMMUNICATIONS_PORT);
//	}

	pthread_t thread_id;
	pthread_create(&thread_id, NULL, funcionDelHilo, NULL);
	//	pthread_join(thread_id, NULL);

	//TODO: en la funcion send_udp_msg esta quitado el checksum para que vaya bien

	while (strcmp(msg, "DISCONNECT") != 0) {

		read_udp_message(socket_fd, msg, strlen(msg));

		if (strncmp(msg, "POWER_MINMAX", 12) == 0) {
			printf("msg: %s\n", msg);
			dato = getParam(rp, TMR_PARAM_RADIO_POWERMAX);
			float max = (uint16_t) dato / 100;
			powerMinMax[0] = max;

			dato = getParam(rp, TMR_PARAM_RADIO_POWERMIN);
			float min = (uint16_t) dato / 100;
			powerMinMax[1] = min;

			enviar_udp_msg(socket_fd, powerMinMax, PARAMS_PORT);
			bzero(powerMinMax, sizeof(powerMinMax));

		} else if (strcmp(msg, "CON_ANT_PORTS") == 0) {
			printf("msg: %s\n", msg);
			getConnectedAntennaPorts(rp, puertosC);

			puertosConect[0] = puertosC[0];
			puertosConect[1] = puertosC[1];
			puertosConect[2] = puertosC[2];
			puertosConect[3] = puertosC[3];

			enviar_udp_msg(socket_fd, puertosConect, PARAMS_PORT);
			bzero(puertosConect, sizeof(puertosConect));
			bzero(puertosC, sizeof(puertosC));

		} else if (strcmp(msg, "ANT_PORTS") == 0) {
			printf("msg: %s\n", msg);
			getAntennaPorts(rp, puertos);

			puertosAnt[0] = puertos[0];
			puertosAnt[1] = puertos[1];
			puertosAnt[2] = puertos[2];
			puertosAnt[3] = puertos[3];

			enviar_udp_msg(socket_fd, puertosAnt, PARAMS_PORT);
			bzero(puertosAnt, sizeof(puertosAnt));
			bzero(puertos, sizeof(puertos));

		} else if (strcmp(msg, "IS_ANT_CHECK_PORT_EN") == 0) {
			printf("msg: %s\n", msg);
			dato = getParam(rp, TMR_PARAM_ANTENNA_CHECKPORT);
			antenaCheck[0] = (int) dato;
			enviar_udp_msg(socket_fd, antenaCheck, PARAMS_PORT);
			bzero(antenaCheck, sizeof(antenaCheck));

		} else if (strncmp(msg, "SET_ANT_CHECK_PORT", 18) == 0) {
			printf("msg: %s\n", msg);
			char* str = NULL;
			char* busca = "true";
			bool value;

			str = strstr(msg, busca);

			if (str) {
				value = true;
				printf("TRUE\n");
				fflush(stdout);
			} else {
				value = false;
				printf("FALSE\n");
				fflush(stdout);
			}
			tmr_ret = TMR_paramSet(rp, TMR_PARAM_ANTENNA_CHECKPORT, &value);

		} else if (strcmp(msg, "GET_POWER") == 0) {
			printf("msg: %s\n", msg);
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
			printf("msg: %s\n", msg);
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
			printf("msg: %s\n", msg);
			getSelectedAntennas(rp, selAnt);
			selecAntenna[0] = selAnt[0];
			selecAntenna[1] = selAnt[1];
			selecAntenna[2] = selAnt[2];
			selecAntenna[3] = selAnt[3];

			enviar_udp_msg(socket_fd, selecAntenna, PARAMS_PORT);
			bzero(selecAntenna, sizeof(selecAntenna));
			bzero(selAnt, sizeof(selAnt));

		} else if (strncmp(msg, "SET_SEL_ANT", 11) == 0) {
			printf("msg: %s\n", msg);
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
			printf("msg: %s\n", msg);
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
			printf("msg: %s\n", msg);

			dato = getParam(rp, TMR_PARAM_REGION_ID);
			//printf("REGION: %s\n", dato);
			fflush(stdout);
			enviar_udp_msg(socket_fd, (char *) dato, PARAMS_PORT);

			dato = getParam(rp, TMR_PARAM_GEN2_TARI);
			//printf("TARI: %s\n", dato);
			fflush(stdout);
			enviar_udp_msg(socket_fd, (char *) dato, PARAMS_PORT);

			dato = getParam(rp, TMR_PARAM_GEN2_BLF);
			//printf("BLF: %s\n", dato);
			fflush(stdout);
			enviar_udp_msg(socket_fd, (char *) dato, PARAMS_PORT);

			dato = getParam(rp, TMR_PARAM_GEN2_TAGENCODING);
			//printf("ENCONDING: %s\n", dato);
			fflush(stdout);
			enviar_udp_msg(socket_fd, (char *) dato, PARAMS_PORT);

			dato = getParam(rp, TMR_PARAM_GEN2_Q);
			//printf("Q: %s\n", dato);
			fflush(stdout);
			enviar_udp_msg(socket_fd, (char *) dato, PARAMS_PORT);

			dato = getParam(rp, TMR_PARAM_GEN2_SESSION);
			//printf("SESION: %s\n", dato);
			fflush(stdout);
			enviar_udp_msg(socket_fd, (char *) dato, PARAMS_PORT);

			dato = getParam(rp, TMR_PARAM_GEN2_TARGET);
			//printf("TARGET: %s\n", dato);
			fflush(stdout);
			enviar_udp_msg(socket_fd, (char *) dato, PARAMS_PORT);

		} else if (strncmp(msg, "SET_REGION", 10) == 0) {
			printf("msg: %s\n", msg);
			int longitud = strlen(msg) - 11;
			char *nuevo = (char*) malloc(sizeof(char) * (longitud + 1));
			nuevo[longitud] = '\0';
			strncpy(nuevo, msg + 11, longitud);
			printf("REGION: %s\n", nuevo);
			fflush(stdout);
			int value = getRegionNumber(nuevo);
			tmr_ret = TMR_paramSet(rp, TMR_PARAM_REGION_ID, &value);

		} else if (strcmp(msg, "GET_SUPPORTED_REGIONS") == 0) {
			printf("msg: %s\n", msg);
			getRegionNames(rp, reg);
			for (int i = 0; i < 20; i++) {
				regiones[i] = reg[i];
				//printf("Reg Nums: %d", reg[i]);
			}
			enviar_udp_msg(socket_fd, regiones, PARAMS_PORT);
			bzero(regiones, sizeof(regiones));

		} else if (strncmp(msg, "SET_TARI", 8) == 0) {
			printf("msg: %s\n", msg);
			int longitud = strlen(msg) - 9;
			char *nuevo = (char*) malloc(sizeof(char) * (longitud + 1));
			nuevo[longitud] = '\0';
			strncpy(nuevo, msg + 9, longitud);
			printf("SET TARI: %s\n", nuevo);
			fflush(stdout);

			int32_t value = getID(nuevo, "Tari");
			printf("TARI ID: %d\n", value);
			tmr_ret = TMR_paramSet(rp, TMR_PARAM_GEN2_TARI, &value);

		} else if (strncmp(msg, "SET_BLF", 7) == 0) {
			printf("msg: %s\n", msg);
			int longitud = strlen(msg) - 8;
			char *nuevo = (char*) malloc(sizeof(char) * (longitud + 1));
			nuevo[longitud] = '\0';
			strncpy(nuevo, msg + 8, longitud);
			printf("SET BLF: %s\n", nuevo);
			fflush(stdout);

			int32_t value = atoi(nuevo);
			printf("BLF ID: %d\n", value);
			tmr_ret = TMR_paramSet(rp, TMR_PARAM_GEN2_BLF, &value);

		} else if (strncmp(msg, "SET_M", 5) == 0) {
			printf("msg: %s\n", msg);
			int longitud = strlen(msg) - 6;
			char *nuevo = (char*) malloc(sizeof(char) * (longitud + 1));
			nuevo[longitud] = '\0';
			strncpy(nuevo, msg + 6, longitud);
			printf("SET M: %s\n", nuevo);
			fflush(stdout);

			int32_t value = getID(nuevo, "M");
			printf("M ID: %d\n", value);
			tmr_ret = TMR_paramSet(rp, TMR_PARAM_GEN2_TAGENCODING, &value);

		} else if (strncmp(msg, "SET_Q", 5) == 0) {
			printf("msg: %s\n", msg);

		} else if (strncmp(msg, "SET_SESSION", 11) == 0) {
			printf("msg: %s\n", msg);
			int longitud = strlen(msg) - 12;
			char *nuevo = (char*) malloc(sizeof(char) * (longitud + 1));
			nuevo[longitud] = '\0';
			strncpy(nuevo, msg + 12, longitud);
			printf("SESION: %s\n", nuevo);
			fflush(stdout);

			int32_t value = atoi(nuevo);
			tmr_ret = TMR_paramSet(rp, TMR_PARAM_GEN2_SESSION, &value);

		} else if (strncmp(msg, "SET_TARGET", 10) == 0) {
			printf("msg: %s\n", msg);
			int longitud = strlen(msg) - 11;
			char *nuevo = (char*) malloc(sizeof(char) * (longitud + 1));
			nuevo[longitud] = '\0';
			strncpy(nuevo, msg + 11, longitud);
			printf("SET TARGET: %s\n", nuevo);
			fflush(stdout);

			int32_t value = getID(nuevo, "Target");
			printf("Target ID: %d\n", value);
			tmr_ret = TMR_paramSet(rp, TMR_PARAM_GEN2_TARGET, &value);

		} else if (strcmp(msg, "START_READING") == 0) {
			while (strcmp(msg, "STOP_READING") != 0) {
				//printf("msg: %s\n", msg);
				//Init tag count
				tag_read_count = 0;
				numeroEPCs = 0;

				//Read tags
				tmr_ret = TMR_readIntoArray(rp, 500, &tag_read_count,
						&tagReads);
				//printf("tag read count %d\n", tag_read_count);
				if (tmr_ret != TMR_SUCCESS) {
					printf("geo_rfid: ERROR READING TAGS INTO ARRAY");
					fflush(stdout);
					errors++;
				} else {
					for (i = 0; i < tag_read_count; i++) {
						TMR_TagReadData* trd;
						char epcStr[128];

						trd = &tagReads[i];
						TMR_bytesToHex(trd->tag.epc, trd->tag.epcByteCount,
								epcStr);
						//printf("EPC:%s ant:%d count:%u rssi: %02x\n", epcStr, trd->antenna, trd->readCount, trd->rssi);
						//printf("RSSI:%d\n", trd->rssi);
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
							rfid_report_msg[j + 2] = epcStr[j];
						}

						send_udp_msg(socket_fd, IP_ADDRESS, RFID_PORT,
								rfid_report_msg, strlen(epcStr) + 2);
						uint8_t antena = trd->antenna;
						int32_t rssi = trd->rssi;
						addTag(epcStr, antena, rssi);
					}
				}
				insertintoDB();

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

//		//Configuración gprs
//		if (gprs_get_config() /*&& FD_ISSET(fd_sim808, &wfds)*/) {
//			gprs_configure_AT();
//		}
//
//		printf("GPS_STATUS %d\n", gps_get_status());
//		printf("GPRS_STATUS %d\n",gprs_get_status());
//		fflush(stdout);
	}

	TMR_destroy(rp);
	return EXIT_SUCCESS;
} // MAIN END

void addTag(char epc[], uint8_t ant, int32_t rssi) {

	char hora[9];
	time_t rawtime;
	struct tm * timeinfo;

	time(&rawtime);
	timeinfo = localtime(&rawtime);
	strftime(hora, 9, "%T", timeinfo);

	strcpy(tabla[numeroEPCs].datos[1].hora, hora);
	strcpy(tabla[numeroEPCs].EPC, epc);
	tabla[numeroEPCs].datos[1].RSSI = rssi;
	tabla[numeroEPCs].datos[1].antena = ant;
	//printf("EPC %s RSSI %d ANT %u HORA %s ", tabla[numeroEPCs].EPC, tabla[numeroEPCs].datos[1].RSSI, tabla[numeroEPCs].datos[1].antena, tabla[numeroEPCs].datos[1].hora);
	tabla[numeroEPCs].numEPC = numeroEPCs;
	numeroEPCs++;
	//printf("NUMERO EPCS %d\n", numeroEPCs);
}

void insertintoDB() {

//	printf("INSERT DB\n");
	char *sqlInsert[70];
	int rc;
	char *zErrMsg = 0;

	for (int i = 0; i < numeroEPCs; ++i) {
		//printf("FOR %d %s %s\n", i, tabla[i].EPC, tabla[i].datos[0].hora);
		sprintf(sqlInsert,
				"INSERT INTO PRUEBAEPC (EPC, HORA, ENVIADO) VALUES (\"%s\" , \"%s\", 0);",
				tabla[i].EPC, tabla[i].datos[1].hora);
//		printf("SQLInsert %s\n", sqlInsert);
		/* Execute SQL statement */
		rc = sqlite3_exec(db, sqlInsert, callback, 0, &zErrMsg);

		if (rc != SQLITE_OK) {
			fprintf(stderr, "SQL error: %s\n", zErrMsg);
			sqlite3_free(zErrMsg);
		} else {
			fprintf(stdout, "Success\n");
		}
	}
}

static const char *device = "/dev/spidev0.0";
static uint32_t speed = 115200;
#define ARRAY_SIZE(a) (sizeof(a) / sizeof((a)[0]))
char *ReadyMsg = "READY";
struct datosBD datSelect[50];
struct recibidoEPC rec[50];
int numGuardado;
int numTuplas;
char * listaEPC;


void *funcionDelHilo(void *parametro) {

	char arrayEPC[24];
	int jj = 0;

	while (1) {
		printf("Funcion del hilo\n");

		selectDataDB();

		while(jj < numTuplas){
				printf("EPC: %s i %d\n", listaEPC[jj],jj);
				jj++;
		//		fflush();
			}

		enviarEPCporSMS();

		//Para enviar por LORA
//		enviarEPCporSPI();
//		sleep(1);
//		for (int i = 1; i < numGuardado; i++) {
//			strncpy(arrayEPC, rec[i].EPC,24);
//			printf("arrayEPC %s\n", arrayEPC);
////			updateDB(arrayEPC);
//			sleep(1);
//		}
		sleep(20);
	}
}

void enviarEPCporSPI() {

//	char *epc;
	char epc[24];
	char readBuffer[24];

	printf("NUM TOTAL DATOS %d\n", numTuplas);
	openspi(device, speed);

	for (int d = 0; d < numTuplas; d++) {
		printf("NUM D %d\n", d);

		strncpy(epc, datSelect[d].EPC, 24);
		transfer(epc, readBuffer, sizeof(epc));
		strncpy(rec[numGuardado].EPC, readBuffer,24);
		printf("Guardado %s\n", rec[numGuardado].EPC);

		numGuardado++;
		if (numGuardado == 50) {
			numGuardado = 0;
		}

		bzero(readBuffer, sizeof(readBuffer));
		bzero(epc, sizeof(epc));
		sleep(1);
	}

	readfromspi(0, 0, 24, readBuffer);
	strncpy(rec[numGuardado].EPC, readBuffer,24);
	printf("Guardado %s\n", rec[numGuardado].EPC);
	bzero(readBuffer, sizeof(readBuffer));
	numGuardado++;
	if (numGuardado == 50) {
		numGuardado = 0;
	}

	closespi();

}

void enviarEPCporSMS(){
	gps_at_send_data(&listaEPC, numTuplas);

}

static int callbackSelect(void *data, int argc, char **argv, char **azColName) {

	char epc[24];
	char readBuffer[24];

//	strcpy(epc, argv[1]);
//	sprintf(epc, "%s", epc);
	printf("ARGV %s\n", argv[1]);
//	strcpy(datSelect[numTuplas].EPC, argv[1]);
//	strcpy(listaEPC[numTuplas], argv[1]);
	listaEPC[numTuplas] = argv[1];
	printf("Lista %s\n", listaEPC[numTuplas]);
//	printf("Datos select %s\n", datSelect[numTuplas].EPC);
	numTuplas++;
	listaEPC = malloc(numTuplas * sizeof(char));


	return 0;
}

void selectDataDB() {

	int rc;
	char *zErrMsg = 0;
	char *sql;
	const char* data = "Callback function called";

	/* Create SQL statement */
	sql = "SELECT * FROM PRUEBAEPC WHERE ENVIADO = 0 GROUP BY EPC;";

	numGuardado = 0;
	numTuplas = 0;
	/* Execute SQL statement */
	rc = sqlite3_exec(db, sql, callbackSelect, (void*) data, &zErrMsg);

	if (rc != SQLITE_OK) {
		fprintf(stderr, "SQL error: %s\n", zErrMsg);
		sqlite3_free(zErrMsg);
	} else {
		fprintf(stdout, "Operation done successfully\n");
	}
	fflush(stdout);
}

void updateDB(char epc[24]) {
//	printf("UPDATE DB\n");

	int rc;
	char *zErrMsg = 0;
	char *sql[100];
	const char* data = "Callback function called";

//	sprintf(sql, "UPDATE PRUEBAEPC SET ENVIADO = 1 WHERE EPC = \"%s\"; SELECT * FROM PRUEBAEPC GROUP BY EPC;", epc);
	sprintf(sql, "UPDATE PRUEBAEPC SET ENVIADO = 1 WHERE EPC = \"%s\";", epc);
	printf("%s\n", sql);

	/* Execute SQL statement */
	rc = sqlite3_exec(db, sql, callback, (void*) data, &zErrMsg);

	if (rc != SQLITE_OK) {
		fprintf(stderr, "SQL error: %s\n", zErrMsg);
		sqlite3_free(zErrMsg);
	} else {
		fprintf(stdout, "Operation done successfully\n");
	}
}
