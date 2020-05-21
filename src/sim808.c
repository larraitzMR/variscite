/*
 * sim808.c
 *
 *  Created on: Apr 1, 2020
 *  Author: Larraitz
 *
 */

#include "sim808.h"
#include "network.h"

// =======================================================================
//
//  SHARED VARIABLES
//  get and/or set functions defined to access these variables from main
// =======================================================================
t_position latitude;
t_position longitude;
unsigned short gprs_status;
unsigned short gps_status;

//Flags
int gprs_configure; //1 if we enter into configuration mode

int fd_gprs; //File descriptor for gprs UART
char gprs_imei[17];
char gps_utc_time[19];

// =======================================================================
//
//  GLOBAL VARIABLES
//
// =======================================================================
unsigned short gprs_prev_status;

int gprs_config_mal;
int gprs_send_mal;
int gprs_critical_errors;
int gprs_buffer_full;
t_frame frame_buffer[GPRS_FRAME_BUFFER_SIZE];
int idx_input;
int idx_output;

// =======================================================================
//
//  PRIVATE FUNCTION DECLARATION
//
// =======================================================================

int recibidoOK = 0;
int recibidoEscribe = 0;
int notCops = 0;

// =======================================================================
//
//  LIBRARY FUNCTIONS
//
// =======================================================================

/* GPRS_INIT: This function turns on the module, configures the uart
 * 			  and initializes all necessary variables
 * 			  Returns uart file descriptor or gprs in case of error.
 */
int gprs_init(void) {
	//Init global variables: A copy of this initialization must be performed in gprs_at_full_functionality
	gprs_status = GPRS_OFF;
	gprs_prev_status = GPRS_OFF;
	gps_status = GPS_OFF;
	gprs_configure = 0;
	gprs_config_mal = 0;
	gprs_critical_errors = 0;
	gprs_buffer_full = 0;
	idx_input = 0;
	idx_output = 0;

	latitude.dato = 0;
	longitude.dato = 0;

	//Export and configure gpio for RESET pin
	if (gpio_config(SRES, GPIO_OUTPUT) < 0) {
		printf("Error configuring RESET pin for SIM808\n");
		fflush(stdout);
		return (-1);
	}

	//Set reset pin high
	//gpio_write(SRES,1);

	//Export and configure gpio for POWERKEY pin
	if (gpio_config(POWERKEY, GPIO_OUTPUT) < 0) {
		printf("Error configuring PWRKEY pin for SIM808\n");
		fflush(stdout);
		return (-1);
	}

	//Turn on module
	//gpio_write(POWERKEY,0);
	//sleep(1);
	//gpio_write(POWERKEY,1);

	//Configure uart
	fd_gprs = uart_open(UART_GPRS, 115200, "8N1");
	if (fd_gprs < 0) {
		printf("Error configuring UART for SIM808\n");
		fflush(stdout);
		return (-1);
	}

#ifdef GPRS_DEBUG
	printf("File descriptor in sim808: %d\n", fd_gprs);
	fflush(stdout);
#endif

	//Enter low consumption mode: Do it from main

	return fd_gprs;
} // GPRS INIT END

/* GPRS_CLOSE: This function closes the gprs uart and turns off the module */
void gprs_close(void) {
	//Close uart
	uart_close(fd_gprs);

	//Turn off module
	gpio_write(POWERKEY, 0);
	sleep(1);
	gpio_write(POWERKEY, 1);

} // GPRS CLOSE END

/* GPRS_PROCESS_MSG: Parse responses to AT commands received from GPRS module.
 * 					 Changes state machine status as a result.
 */
void gprs_process_msg(void) {
	char gprs_msg[255];
	char separators[4] = "\r\n";
	char separators2[6] = ",:\r\n";
	char separators3[9] = "+,:()\"\r\n";
	//char separators3[9] = "()";
	char *temp;
	//char temp2[128];
	char *temp3;

	char msg[100];

	//Clean buffer
	bzero(&gprs_msg, sizeof(gprs_msg));

	//Read (all) from uart
	while (uart_read(fd_gprs, gprs_msg, sizeof(gprs_msg)) > 0) {
		//TODO: If we receive a string starting with * (0x2a) we can consider it is a message from our server
		//Separate lines by NL and CR
		temp = strtok(gprs_msg, separators);
		//while (temp != NULL){
		if (temp != NULL) {

#ifdef GPRS_DEBUG
			if (strlen(temp) > 1) {
				printf("TEMP %s\n", temp);
				fflush(stdout);
			}
#endif
			if (temp[0] == '>') {
				recibidoEscribe = 1;
				printf("recibido escribe\n");
				//Signal for us to send tcp message
			} else if (strncmp("86", temp, 2) == 0) {
				bzero(&gprs_imei, sizeof(gprs_imei));
				//Recibido imei: Sacamos numero de serie
				strncpy(gprs_imei, temp, strlen(temp));
				if (gprs_prev_status == GPRS_TEST_OK) {
					gprs_status = GPRS_IMEI_OK;
				}
			} else if (strncmp("+CREG:", temp, 6) == 0) {
				if ((temp[9] == '1') || (temp[9] == '5')) {
					//Registrado o registrado en roaming
					gprs_status = GPRS_NETWORK_OK;
				} else if (temp[9] == 3) {
					//Register denied
					if (gprs_critical_errors <= GPRS_CRITICAL_ERROR_LIMIT)
						gprs_critical_errors++;
					if (gprs_critical_errors == GPRS_CRITICAL_ERROR_LIMIT) {
						gprs_status = GPRS_ERROR;
						gprs_configure = 0;
						printf(
								"Max GPRS critical errors reached. Recommended to use another network interface.\n");
						fflush(stdout);
						//Messages in buffer will be lost
						idx_input = idx_output;
					} else {
						//Not registered, keep waiting
						gprs_status = gprs_prev_status;
					}
				} else {
					//Not registered, keep waiting
					gprs_status = gprs_prev_status;
				}
			} else if (strncmp("OK", temp, 2) == 0) {

				recibidoOK = 1;
				printf("recibido ok\n");
			
			} else if ((strncmp("+CGNSINF:", temp, 9) == 0)
					|| (strncmp("+UGNSINF:", temp, 9) == 0)) {
				//Format: +CGNSINF: <GNSSS run status>, <fix status>, <UTC Date & time>, <latitude>, <longitude> .....
				temp3 = strtok(temp, separators2); //+CGNSINF
				temp3 = strtok(NULL, separators2); //run status
				if (temp3[0] == '1') {
					temp3 = strtok(NULL, separators2); //fix status
					if (temp3[0] == '1') {
						//Get date and position
						gps_status = GPS_OK;
						temp3 = strtok(NULL, separators2); //utc date & time
						bzero(&gps_utc_time, sizeof(gps_utc_time));
						strncpy(gps_utc_time, temp3, 18);
						temp3 = strtok(NULL, separators2); //latitude
						latitude.dato = atof(temp3);
						temp3 = strtok(NULL, separators2); //longitude
						longitude.dato = atof(temp3);
					} else {
						gps_status = GPS_NO_COVERAGE;
					}
				} else {
					gps_status = GPS_NO_COVERAGE;
				}
			} else if ((strncmp("+COPS:", temp, 6) == 0)) {
				//ENVIAR DIRECTAMENTE EL TEMP.
				sprintf(msg, "%s\n", temp);
				send_tcp_message(msg);
				//send_msg(temp);
				int cont = 0; /*
				while (cont < 5) {
					send_tcp_message(temp);
					cont++;
				}*/
				//send_tcp_message(temp);
				//Format: +COPS: (1,"Retevision Movil","AMENA","21403"),(2,"MOVISTAR","MSTAR","21407"),(1)
				notCops = 1;
				/*temp3 = strtok(temp, separators3); //+CGNSINF
				printf("COPS %s\n", temp3);
				temp3 = strtok(NULL, separators3); //run status
				int i = 0;
				char* cops;
				//bzero(cops, 10);

				if(temp3 != NULL){
					 while(temp3 != NULL){
					 	//strcpy(cops[i], temp3);
					 	printf("Token: %s\n", temp3);
            			temp3 = strtok(NULL, separators3);
            			i++;
            		}
				}*/
			} else if (strncmp("+PDP: DEACT", temp, 11) == 0) {
				if ((gprs_status != GPRS_ERROR)
						&& (gprs_status != GPRS_LOW_POWER)) {
					gprs_status = GPRS_PDP_DEACT;
					gprs_configure = 1;
				}

			} else if ((strncmp("ERROR", temp, 5) == 0)
					|| (strncmp("+CME ERROR", temp, 10) == 0)) {
				if (gprs_critical_errors <= GPRS_CRITICAL_ERROR_LIMIT)
					gprs_critical_errors++;
				if (gprs_critical_errors == GPRS_CRITICAL_ERROR_LIMIT) {
					gprs_status = GPRS_ERROR;
					gprs_configure = 0;
					printf(
							"Max GPRS critical errors reached. Recommended to use another network interface.\n");
					fflush(stdout);
					//Messages in buffer will be lost
					idx_input = idx_output;
				} else if (gprs_configure) {
					//If we don't reach max. allowed errors, repeat last command.
					if (gprs_status < GPRS_CONTEXT_OK) {
						gprs_status = gprs_prev_status;
					} else {
						gprs_status = GPRS_PDP_DEACT;
					}
				}
			} else {
				//Do nothing with other messages
#ifdef GPRS_DEBUG
				if (strlen(temp) > 1) {
					printf("NO PARSEADO: *%s*\n", temp);
					fflush(stdout);
				}
#endif
			}
			//Get next token
			//temp = strtok(NULL,separators);
		}
		//Clean buffer
		bzero(&gprs_msg, sizeof(gprs_msg));
	}
} // GPRS PROCESS MSG END

/* GPRS AT TEST: Send AT command for testing communication with module
 * 				 As a result, module will adjust auto-baudrate
 */
void gprs_at_test(void) {
	char *at_cmd = "AT\r\n";

	int result = uart_write_buffer(fd_gprs, at_cmd, strlen(at_cmd));
	printf("%s\n", at_cmd);

	//usleep(T_100MS);

	//result = uart_write_buffer(fd_gprs, at_cmd, strlen(at_cmd));

#ifdef GPRS_DEBUG
	printf("%s\n", at_cmd);
	if (result < 0) {
		printf("GPRS: Error enviando comando AT\n");
	}
	fflush(stdout);
#endif

	gprs_status = GPRS_IDLE;
	gprs_prev_status = GPRS_OFF;
} // GPRS AT TEST END


/* GPS_AT_SEND_DATA: Send data to mobile */
//void gps_at_send_data(char *epc, struct datosBD *dBD) {
void gps_at_send_data(char *epc[], int numTuplas) {

	char gprs_msg[26];
	int i = 0;

	char *at_cmd = "AT+CMGF=1\r";
	printf("%s\n", at_cmd);
	int result = uart_write_buffer(fd_gprs, at_cmd, strlen(at_cmd));

	sleep(1);

	gprs_process_msg();

	while (!recibidoOK) {
		printf("ESPERANDO OK");
	}
	recibidoOK = 0;

	sleep(1);

	printf("ESPERANDO");
	at_cmd = "AT+CMGS=\"+34649103025\"\r";
	printf("%s\n", at_cmd);
	result = uart_write_buffer(fd_gprs, at_cmd, strlen(at_cmd));

	gprs_process_msg();

	sleep(2);

	while (i < numTuplas) {
//		printf("%s i %d\n", epc[i], i);
//		sprintf(at_cmd, "%s\n", epc[i]);
//		strcpy(at_cmd, epc[i]);
		at_cmd = epc[i];
		printf("%s\n", at_cmd);
		sprintf(gprs_msg, "%s\r", at_cmd);
		printf("%s\n", gprs_msg);
		result = uart_write_buffer(fd_gprs, gprs_msg, strlen(gprs_msg));
		i++;
//		result = uart_write_buffer(fd_gprs, "\r", 3);
		sleep(2);
		bzero(&gprs_msg, sizeof(gprs_msg));
	}

	at_cmd = "\x1a";
	printf("%s\n", at_cmd);
	result = uart_write_buffer(fd_gprs, at_cmd, strlen(at_cmd));

//	at_cmd = epc;
//	printf("%s\n", at_cmd);
//	result = uart_write_buffer(fd_gprs, at_cmd, strlen(at_cmd));

} // GPS AT GET DATA

void send_msg(char* message)
{
	char msg[10];

	//read_tcp_message(msg);
	while(strncmp(msg, "RECIBIDO", 8) == 0){
		send_tcp_message(message);
		read_tcp_message(msg);
	}
}

void gprs_get_network(char* message){

	char gprs_msg[26];
	int i = 0;
	char separators[4] = "\r\n";
	char separators2[5] = ":\r\n ";
	char separators3[9] = "()";
	char* temp;
	char* temp3;

	char *at_cmd = "AT+COPS=?\r";
	printf("%s\n", at_cmd);
	int result = uart_write_buffer(fd_gprs, at_cmd, strlen(at_cmd));
	while(notCops == 0) {
		//printf("Processing\n");
		gprs_process_msg();
		
	}
	
}

void gprs_set_network(char* network){
 
	char gprs_msg[26];
	int i = 0;
	printf("SET NETWORK\n"); 
	char *at_cmd = "AT+COPS=\r";
	sprintf(gprs_msg, "AT+COPS=4,2,%s\r", network);
	printf("%s\n", gprs_msg); 
	int result = uart_write_buffer(fd_gprs, at_cmd, strlen(at_cmd));
	while (!recibidoOK) {
		gprs_process_msg();
	}
	
}

void gprs_send_SMS(char* number, char* text){

	char gprs_msg[26];
	int i = 0;

	char *at_cmd = "AT+CMGF=1\r";
	printf("%s\n", at_cmd);
	int result = uart_write_buffer(fd_gprs, at_cmd, strlen(at_cmd));
	sleep(1);

	gprs_process_msg();

	while (!recibidoOK) {
		gprs_process_msg();
	}

	printf("ESPERANDO\r\n");
	at_cmd = "AT+CMGS=";
	//printf("%s\n", at_cmd);
	//sprintf(gprs_msg, "%s", at_cmd);
	//printf("%s\n", gprs_msg);
	sprintf(gprs_msg, "%s\"%s\"\r",at_cmd, number);
	//sprintf(at_cmd, "%s\r", number)	;
	//strcat(at_cmd, number);
	//at_cmd = "AT+CMGS=\"+34649103025\"\r";
	printf("%s\n", gprs_msg);
	result = uart_write_buffer(fd_gprs, gprs_msg, strlen(gprs_msg));

	gprs_process_msg();

	//sleep(2);
	sprintf(gprs_msg, "%s\r", text);
	printf("%s\n", gprs_msg);
	result = uart_write_buffer(fd_gprs, gprs_msg, strlen(gprs_msg));

	at_cmd = "\x1a";
	printf("%s\n", at_cmd);
	result = uart_write_buffer(fd_gprs, at_cmd, strlen(at_cmd));
}


