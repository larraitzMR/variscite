/*
 * sim808.c
 *
 *  Created on: Oct 2, 2017
 *  Author: xabi1.1
 *
 *  Attention: If sleep mode causes any trouble, a single character must be send
 *  		   before any at command, waiting 100ms between single character and at.
 */

#include "sim808.h"

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
void gprs_at_test(void);
void gprs_at_allow_sleep(void);
void gprs_at_enter_pin(void);
void gprs_at_check_network(void);
void gprs_at_set_context(void);
void gprs_at_set_APN(char *apn, char *user, char *pass);
void gprs_at_network_up(void);
void gprs_at_get_ip(void);
void gprs_at_shut(void);
void gprs_at_close_con(void);

void gprs_at_open_tcp_con(char *ip, unsigned int port);
int gprs_is_ip_v4(char* ip);
unsigned char gprs_checksum(unsigned char *data, short len);
void gprs_send_frame_in_buffer(void);

int recibidoOK = 0;
int recibidoEscribe = 0;

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

/* GPRS_CONFIGURE_AT: Configure GPRS via AT commands. Implements a state machine
 * 					  This function must be called twice, since first time, just
 * 					  basic tests are performed, and sleep configured if available.
 * 					  With basic configuration, only GPS is available for use.
 * */
void gprs_configure_AT(void) {
	switch (gprs_status) {
	case GPRS_OFF:
		gprs_config_mal = 0;
		gprs_at_test();
		break;
	case GPRS_TEST_OK:
		gprs_config_mal = 0;
		gprs_at_imei();
		break;
	case GPRS_IMEI_OK:
		gprs_config_mal = 0;
#if defined(SUPPORT_SLEEP)
		gprs_at_allow_sleep();
#elif defined(PIN_REQUIRED)
		gprs_at_enter_pin();
#else
		gprs_at_check_network();
#endif
		break;
	case GPRS_SLEEP_ALLOWED_OK:
		gprs_config_mal = 0;
#if defined(PIN_REQUIRED)
		gprs_at_enter_pin();
#else
		gprs_at_check_network();
#endif
		break;
	case GPRS_PIN_OK:
		gprs_config_mal = 0;
		gprs_at_check_network();
		break;
	case GPRS_NETWORK_OK:
		gprs_config_mal = 0;
		gprs_at_set_context();
		break;
	case GPRS_CONTEXT_OK:
		gprs_config_mal = 0;
		gprs_at_set_APN(GPRS_APN_NAME, GPRS_APN_USER, GPRS_APN_PASS);
		break;
	case GPRS_APN_READY:
		gprs_config_mal = 0;
		gprs_at_network_up();
		break;
	case GPRS_NETWORK_UP:
		gprs_config_mal = 0;
		gprs_at_get_ip();
		break;
	case GPRS_PDP_DEACT:
		gprs_config_mal = 0;
		gprs_at_shut();
		break;
	default:
		usleep(T_100MS);
		if (gprs_config_mal <= GPRS_CONFIG_MAL_LIMIT)
			gprs_config_mal++;
		if (gprs_config_mal == GPRS_CONFIG_MAL_LIMIT) {
			//Configuration process ended with critical errors
			gprs_status = GPRS_ERROR;
			gprs_configure = 0;
			printf("GPRS: Error limit during configuration reached\n");
			fflush(stdout);
		}
		break;
	}
} // END GPRS CONFIGURE AT

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
				//If we are in config mode, move to next state
				if ((gprs_configure) && (gprs_status == GPRS_IDLE)) {
					switch (gprs_prev_status) {
					case GPRS_OFF:
						gprs_status = GPRS_TEST_OK;
						//#if !defined(SUPPORT_SLEEP)
						//gprs_configure=0;
						//#endif
						break;
					case GPRS_IMEI_OK:
#if defined(SUPPORT_SLEEP)
						gprs_status=GPRS_SLEEP_ALLOWED_OK;
						gprs_configure=0;
#elif defined(PIN_REQUIRED)
						gprs_status=GPRS_PIN_OK;
#else
						gprs_status = GPRS_NETWORK_OK;
#endif
						break;
					case GPRS_SLEEP_ALLOWED_OK:
#if defined(PIN_REQUIRED)
						gprs_status=GPRS_PIN_OK;
#else
						gprs_status = GPRS_NETWORK_OK;
#endif
						break;
					case GPRS_PIN_OK:
						gprs_status = GPRS_NETWORK_OK;
						break;
					case GPRS_NETWORK_OK:
						gprs_status = GPRS_CONTEXT_OK;
						break;
					case GPRS_CONTEXT_OK:
						gprs_status = GPRS_APN_READY;
						break;
					case GPRS_APN_READY:
						gprs_status = GPRS_NETWORK_UP;
						break;
						//case GPRS_NETWORK_UP:
						//	gprs_status=GPRS_IP_OK;
						//	break;
					default:
						break;
					} // End switch
				}

				//TODO: OK response to other commands, different from configuration
			} else if (gprs_is_ip_v4(temp)) {
				//Local ip received
				gprs_critical_errors = 0;
				if ((gprs_configure) && (gprs_status == GPRS_IDLE)
						&& (gprs_prev_status == GPRS_NETWORK_UP)) {
					gprs_status = GPRS_IP_OK;
					gprs_configure = 0;
				}
			} else if (strncmp("CONNECT OK", temp, 10) == 0) {
				gprs_critical_errors = 0;
				gprs_status = GPRS_TCP_READY;
				if (idx_output != idx_input) {
					//Send message from buffer
					gprs_send_frame_in_buffer();
				}
			} else if (strncmp("CLOSE OK", temp, 8) == 0) {
				gprs_status = GPRS_IP_OK;
				idx_input = idx_output; //Just to be sure that everything is ok
			} else if (strncmp("CLOSED", temp, 6) == 0) {
				gprs_status = GPRS_IP_OK;
				//In this case, some messages can be lost
				if (idx_input != idx_output) {
					printf(
							"TCP connection closed unexpectedly: Some frames will be lost\n");
					fflush(stdout);
					idx_input = idx_output;
				}
			} else if (strncmp("SEND OK", temp, 7) == 0) {
				gprs_critical_errors = 0;
				//Update buffer index
				idx_output = (idx_output + 1) % GPRS_FRAME_BUFFER_SIZE;
				gprs_send_mal = 0;
				if (idx_input != idx_output) {
					//If there is more messages in buffer, send another one
					gprs_send_frame_in_buffer();
				} else {
					//In other case, close connection
					gprs_at_close_con();
				}
			} else if (strncmp("SEND FAIL", temp, 9) == 0) {
				//Don't update buffer index
				gprs_send_mal++;
				if (gprs_send_mal == GPRS_SEND_MAL_LIMIT) {
					//Status to PDP DEACT, so we are forced to reset connection
					if (gprs_status != GPRS_ERROR) {
						gprs_status = GPRS_PDP_DEACT;
						gprs_configure = 1;
					}

					//Messages will be lost
					printf(
							"Restarting network connection: Some frames will be lost\n");
					fflush(stdout);
					idx_input = idx_output;
				}
			} else if (strncmp("SHUT OK", temp, 7) == 0) {
				if (gprs_status != GPRS_ERROR) {
					gprs_status = GPRS_NETWORK_OK;
					gprs_configure = 1;
				}
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

				//Format: +COPS: (1,"Retevision Movil","AMENA","21403"),(2,"MOVISTAR","MSTAR","21407"),(1)
				temp3 = strtok(temp, separators3); //+CGNSINF
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
				}
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

/* GPRS AT IMEI: Send AT command to request imei */
void gprs_at_imei(void) {
	char *at_cmd = "AT+CGSN\r";

	int result = uart_write_buffer(fd_gprs, at_cmd, strlen(at_cmd));

#ifdef GPRS_DEBUG
	printf("%s\n", at_cmd);
	if (result < 0) {
		printf("GPRS: Error enviando comando AT+CGSN\n");
	}
	fflush(stdout);
#endif

	gprs_status = GPRS_IDLE;
	gprs_prev_status = GPRS_TEST_OK;
} // GPRS AT IMEI END

/* GPRS_AT_MINIMUM_FUNCTIONALITY: This function is called when initializating GPRS to allow module
 * 								  to enter in a minimum functionality mode with RF systems OFF.
 * 								  GPS can be used. Full configuration is required after exiting this mode.
 */
void gprs_at_minimum_functionality(void) {
	char *at_cmd = "AT+CFUN=0\r";

	int result = uart_write_buffer(fd_gprs, at_cmd, strlen(at_cmd));

#ifdef GPRS_DEBUG
	printf("%s\n", at_cmd);
	if (result < 0) {
		printf("GPRS: Error enviando comando AT+CFUN\n");
	}
	fflush(stdout);
#endif

	gprs_status = GPRS_LOW_POWER;

} // GPRS AT MINIMUM FUNCTIONALITY END

/* GPRS_AT_FULL_FUNCTIONALITY: This module performs a hard reset, resulting in a GPRS module waking up
 * 							   in normal functionality mode. GPRS status and GPS status are reset to PWR_OFF.
 * 							   GPRS enters configuration mode.
 */
void gprs_at_full_functionality(void) {
	char *at_cmd = "AT+CFUN=1,1\r";

	int result = uart_write_buffer(fd_gprs, at_cmd, strlen(at_cmd));

#ifdef GPRS_DEBUG
	printf("%s\n", at_cmd);
	if (result < 0) {
		printf("GPRS: Error enviando comando AT+CFUN\n");
	}
	fflush(stdout);
#endif

	//Init global variables: A copy of this initialization must be performed in gprs_init
	gprs_status = GPRS_OFF;
	gprs_prev_status = GPRS_OFF;
	gps_status = GPS_OFF;
	gprs_configure = 1; //Different from gprs_init
	gprs_config_mal = 0;
	gprs_critical_errors = 0;
	gprs_buffer_full = 0;
	idx_input = 0;
	idx_output = 0;
} // GPRS AT FULL FUNCTIONALITY END

/* GPRS_AT_ALLOW_SLEEP: Puts gprs module in automatic sleep mode (low consumption)
 * 						Module enters sleep if there is no data in uart and no interrupts in gpio.
 * 						Write wake character for exit sleep, and wait 100ms before sending at commands.
 */
void gprs_at_allow_sleep(void) {
	char *at_cmd = "AT+CSCLK=2\r";

	int result = uart_write_buffer(fd_gprs, at_cmd, strlen(at_cmd));

#ifdef GPRS_DEBUG
	printf("%s\n", at_cmd);
	if (result < 0) {
		printf("GPRS: Error enviando comando AT+CGSN\n");
	}
	fflush(stdout);
#endif

	gprs_status = GPRS_IDLE;
	gprs_prev_status = GPRS_IMEI_OK;
} // GPRS AT ALLOW SLEEP END

/* GPRS_AT_ENTER_PIN: Enter pin for sim card */
void gprs_at_enter_pin(void) {
	char at_cmd[255];

	//Wake up module
#if defined(SUPPORT_SLEEP)
	uart_write_buffer(fd_gprs, "a\r", 2);
	usleep(T_100MS);
#endif

	bzero(&at_cmd, 255);
	sprintf(at_cmd, "AT+CPIN=%s\r", "3870");

	int result = uart_write_buffer(fd_gprs, at_cmd, strlen(at_cmd));

#ifdef GPRS_DEBUG
	printf("%s\n", at_cmd);
	if (result < 0) {
		printf("GPRS: Error enviando comando AT+CPIN\n");
	}
	fflush(stdout);
#endif

	gprs_prev_status = gprs_status;
	gprs_status = GPRS_IDLE;
} // GPRS AT ENTER PIN

/* GPRS_AT_CHECK_NETWORK: Send at command to check if module is registered in the network
 * 						  This process might take a while. If negative response, repeat state
 */
void gprs_at_check_network(void) {
	char *at_cmd = "AT+CREG?\r";

	//Wake up module
#if defined(SUPPORT_SLEEP)
	uart_write_buffer(fd_gprs, "a\r", 2);
	usleep(T_100MS);
#endif

	int result = uart_write_buffer(fd_gprs, at_cmd, strlen(at_cmd));

#ifdef GPRS_DEBUG
	printf("%s\n", at_cmd);
	if (result < 0) {
		printf("GPRS: Error enviando comando AT+CREG\n");
	}
	fflush(stdout);
#endif

	gprs_prev_status = gprs_status;
	gprs_status = GPRS_IDLE;

} // GPRS AT CHECK NETWORK END

/* GPRS_AT_SET_CONTEXT: Send at command to set GPRS context */
void gprs_at_set_context(void) {
	char *at_cmd = "AT+CGATT=1\r";

	//Wake up module
#if defined(SUPPORT_SLEEP)
	uart_write_buffer(fd_gprs, "a\r", 2);
	usleep(T_100MS);
#endif

	int result = uart_write_buffer(fd_gprs, at_cmd, strlen(at_cmd));

#ifdef GPRS_DEBUG
	printf("%s\n", at_cmd);
	if (result < 0) {
		printf("GPRS: Error enviando comando AT+CGATT\n");
	}
	fflush(stdout);
#endif

	gprs_status = GPRS_IDLE;
	gprs_prev_status = GPRS_NETWORK_OK;
} // GPRS AT SET CONTEXT END

/* GPRS_AT_SET_APN: Send at command to configure apn information */
void gprs_at_set_APN(char *apn, char *user, char *pass) {
	char at_cmd[255];

#if defined(SUPPORT_SLEEP)
	uart_write_buffer(fd_gprs, "a\r", 2);
	usleep(T_100MS);
#endif

	bzero(&at_cmd, 255);
	sprintf(at_cmd, "AT+CSTT=\"%s\",\"%s\",\"%s\"\r", apn, user, pass);

	int result = uart_write_buffer(fd_gprs, at_cmd, strlen(at_cmd));

#ifdef GPRS_DEBUG
	printf("%s\n", at_cmd);
	if (result < 0) {
		printf("GPRS: Error enviando comando AT+CSTT\n");
	}
	fflush(stdout);
#endif

	gprs_status = GPRS_IDLE;
	gprs_prev_status = GPRS_CONTEXT_OK;
} // GPRS AT SET APN END

/* GPRS_AT_NETWORK_UP: Brings up the gprs wireless connection with the apn previously configured */
void gprs_at_network_up(void) {
	char *at_cmd = "AT+CIICR\r";

#if defined(SUPPORT_SLEEP)
	uart_write_buffer(fd_gprs, "a\r", 2);
	usleep(T_100MS);
#endif

	int result = uart_write_buffer(fd_gprs, at_cmd, strlen(at_cmd));

#ifdef GPRS_DEBUG
	printf("%s\n", at_cmd);
	if (result < 0) {
		printf("GPRS: Error enviando comando AT+CIICR\n");
	}
	fflush(stdout);
#endif

	gprs_status = GPRS_IDLE;
	gprs_prev_status = GPRS_APN_READY;
} // GPRS AT NETWORK UP

/* GPRS_AT_GET_IP: Send at command to ask for local ip address.
 * 				   This step is mandatory.
 */
void gprs_at_get_ip(void) {
	char *at_cmd = "AT+CIFSR\r";

#if defined(SUPPORT_SLEEP)
	uart_write_buffer(fd_gprs, "a\r", 2);
	usleep(T_100MS);
#endif

	int result = uart_write_buffer(fd_gprs, at_cmd, strlen(at_cmd));

#ifdef GPRS_DEBUG
	printf("%s\n", at_cmd);
	if (result < 0) {
		printf("GPRS: Error enviando comando AT+CIFSR\n");
	}
	fflush(stdout);
#endif

	gprs_status = GPRS_IDLE;
	gprs_prev_status = GPRS_NETWORK_UP;
} // GPRS AT GET IP END

/* GPRS_AT_OPEN_TCP_CON: Opens a tcp connection to ip:port */
void gprs_at_open_tcp_con(char *ip, unsigned int port) {
	char at_cmd[255];

#if defined(SUPPORT_SLEEP)
	uart_write_buffer(fd_gprs, "a\r", 2);
	usleep(T_100MS);
#endif

	bzero(&at_cmd, 255);
	sprintf(at_cmd, "AT+CIPSTART=\"TCP\",\"%s\",\"%d\"\r", ip, port);

	int result = uart_write_buffer(fd_gprs, at_cmd, strlen(at_cmd));

#ifdef GPRS_DEBUG
	printf("%s\n", at_cmd);
	if (result < 0) {
		printf("GPRS: Error enviando comando AT+CIPSTART\n");
	}
	fflush(stdout);
#endif

	gprs_status = GPRS_TCP_CONNECTING;
} // GPRS AT OPEN TCP CON END

/* GPRS_AT_SHUT: Send at command to recover from +PDP: DEACT"
 * 				 Change status to GPRS_NETWORK_OK and activate config mode
 */
void gprs_at_shut(void) {
	char *at_cmd = "AT+CIPSHUT\r";

#if defined(SUPPORT_SLEEP)
	uart_write_buffer(fd_gprs, "a\r", 2);
	usleep(T_100MS);
#endif

	int result = uart_write_buffer(fd_gprs, at_cmd, strlen(at_cmd));

#ifdef GPRS_DEBUG
	printf("%s\n", at_cmd);
	if (result < 0) {
		printf("GPRS: Error enviando comando AT+CIPSHUT\n");
	}
	fflush(stdout);
#endif

	gprs_prev_status = gprs_status;
	gprs_status = GPRS_IDLE;
} // GPRS AT SHUT END

/* GPRS_AT_CLOSE_CON: Closes tcp connection via gprs */
void gprs_at_close_con(void) {
	char *at_cmd = "AT+CIPCLOSE\r";

#if defined(SUPPORT_SLEEP)
	uart_write_buffer(fd_gprs, "a\r", 2);
	usleep(T_100MS);
#endif

	int result = uart_write_buffer(fd_gprs, at_cmd, strlen(at_cmd));

#ifdef GPRS_DEBUG
	printf("%s\n", at_cmd);
	if (result < 0) {
		printf("GPRS: Error enviando comando AT+CIPCLOSE\n");
	}
	fflush(stdout);
#endif

	//gprs_status = GPRS_IP_OK; //Updated when received "close ok"
} // GPRS AT CLOSE CON END

/* GPRS_GET_STATUS: Returns GPRS status in internal state machine */
unsigned short gprs_get_status(void) {
	return gprs_status;
} // GPRS GET STATUS END

/* GPS_POWER_ON: Send at command to turn on gps module.
 * 				 If auto = 1, turn on automatic report  */
void gps_power_on(int auto_report) {
	char *at_cmd = "AT+CGNSPWR=1\r";
	char *at_cmd_auto = "AT+CGNSURC=5\r"; //Report every 5 seconds
	char *at_cmd_manual = "AT+CGNSURC=0\r"; //Just report under request

#if defined(SUPPORT_SLEEP)
	uart_write_buffer(fd_gprs, "a\r", 2);
	usleep(T_100MS);
#endif

	int result = uart_write_buffer(fd_gprs, at_cmd, strlen(at_cmd));

#ifdef GPRS_DEBUG
	printf("%s\n", at_cmd);
	if (result < 0) {
		printf("GPRS: Error enviando comando AT+CGNSPWR\n");
	}
	fflush(stdout);
#endif

	usleep(T_100MS);

	if (auto_report) {
		result = uart_write_buffer(fd_gprs, at_cmd_auto, strlen(at_cmd_auto));
	} else {
		result = uart_write_buffer(fd_gprs, at_cmd_manual,
				strlen(at_cmd_manual));
	}

#ifdef GPRS_DEBUG
	if (result < 0) {
		printf("GPRS: Error enviando comando AT+CGNSURC\n");
		fflush(stdout);
	}
#endif

	gps_status = GPS_NO_COVERAGE;
} // GPS POWER ON END

/* GPS_POWER_OFF: Send at command to turn off gps module */
void gps_power_off(void) {
	char *at_cmd = "AT+CGNSPWR=0\r";

#if defined(SUPPORT_SLEEP)
	uart_write_buffer(fd_gprs, "a\r", 2);
	usleep(T_100MS);
#endif

	int result = uart_write_buffer(fd_gprs, at_cmd, strlen(at_cmd));

#ifdef GPRS_DEBUG
	printf("%s\n", at_cmd);
	if (result < 0) {
		printf("GPRS: Error enviando comando AT+CGNSPWR\n");
	}
	fflush(stdout);
#endif

	gps_status = GPS_OFF;
} //GPS POWER OFF END

/* GPS_AT_GET_DATA: Send at command requesting gps real time information */
void gps_at_get_data(void) {
	char *at_cmd = "AT+CGNSINF\r";

#if defined(SUPPORT_SLEEP)
	uart_write_buffer(fd_gprs, "a\r", 2);
	usleep(T_100MS);
#endif

	int result = uart_write_buffer(fd_gprs, at_cmd, strlen(at_cmd));

#ifdef GPRS_DEBUG
	printf("%s\n", at_cmd);
	if (result < 0) {
		printf("GPRS: Error enviando comando AT+CGNSINF\n");
	}
	fflush(stdout);
#endif
} // GPS AT GET DATA

void append(char *s, char c) {
	int len = strlen(s);
	s[len] = c;
	s[len + 1] = '\0';
}

/* GPS_AT_SEND_DATA: Send data to mobile */
//void gps_at_send_data(char *epc) {
//
//	char gprs_msg[255];
//	bzero(&gprs_msg, sizeof(gprs_msg));
//
//	char *at_cmd = "AT+CMGF=1\r";
//	printf("%s\n", at_cmd);
//	int result = uart_write_buffer(fd_gprs, at_cmd, strlen(at_cmd));
//
//	sleep(1);
//
//	gprs_process_msg();
//
//	while(!recibidoOK){
//	}
//
//	sleep(1);
//
//	at_cmd = "AT+CMGS=\"+34649103025\"\r";
//	printf("%s\n", at_cmd);
//	result = uart_write_buffer(fd_gprs, at_cmd, strlen(at_cmd));
//
//	gprs_process_msg();
//
//	sleep(2);
//
//	at_cmd = epc;
//	printf("%s\n", at_cmd);
//	result = uart_write_buffer(fd_gprs, at_cmd, strlen(at_cmd));
//
//	at_cmd = "\x1a";
//	printf("%s\n", at_cmd);
//	result = uart_write_buffer(fd_gprs, at_cmd, strlen(at_cmd));
//
//} // GPS AT GET DATA

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

/* GPS_GET_LATITUDE: Returns gps latitude, previously obtained from gps */
t_position gps_get_latitude(void) {
	return latitude;
} // GPS GET LATITUDE END

/* GPS_GET_LONGITUDE: Returns gps longitude, previously obtained from gps */
t_position gps_get_longitude(void) {
	return longitude;
} // GPS GET LONGITUDE END

/* GPS_GET_STATUS: Return curreng gps status */
unsigned short gps_get_status(void) {
	return gps_status;
} // GPS GET STATUS END

/* GPRS_GET_CONFIG: Return 1 if gprs is in config mode, 0 otherwise. */
int gprs_get_config(void) {
	return gprs_configure;
} // GPRS GET CONFIG END

/* GPRS_SET_CONFIG: Put GPRS in config mode.
 * 					Configuration must be activated in gprs_at_full_functionality
 */
//void gprs_set_config(void){
//	gprs_configure = 1;
//} // GPRS SET CONFIG END

/* GPRS_CHECK_ERROR: Return one if critical error. In that case recommendation is to use any other network interface */
int gprs_check_error(void) {
	if (gprs_status == GPRS_ERROR) {
		return 1;
	} else {
		return 0;
	}
} // GPRS CHECK ERROR END

/* GPRS_SEND_POSITION: Send message to server with position */
int gprs_send_position(t_position lat, t_position lon) {
	//Check if frame buffer is full
	if (gprs_buffer_full) {
#ifdef GPRS_DEBUG
		printf("gprs_send error: Buffer full\n");
		fflush(stdout);
#endif
		gprs_status = GPRS_ERROR;
		gprs_configure = 0;
		idx_input = idx_output;
		return (-1);
	}

	//Check status
	if (gprs_status < GPRS_IP_OK) {
#ifdef GPRS_DEBUG
		printf("gprs_send error: gprs connection missconfigured\n");
		fflush(stdout);
#endif
		gprs_status = GPRS_ERROR;
		gprs_configure = 0;
		idx_input = idx_output;
		return (-1);
	}

	//Prepare frame
	bzero(&frame_buffer[idx_input].data, GPRS_MAX_FRAME_SIZE);

	//Make message
	frame_buffer[idx_input].data[0] = '#';

	//Latitude
	frame_buffer[idx_input].data[1] = lat.byte[0];
	frame_buffer[idx_input].data[2] = lat.byte[1];
	frame_buffer[idx_input].data[3] = lat.byte[2];
	frame_buffer[idx_input].data[4] = lat.byte[3];

	//Longitude
	frame_buffer[idx_input].data[5] = lon.byte[0];
	frame_buffer[idx_input].data[6] = lon.byte[1];
	frame_buffer[idx_input].data[7] = lon.byte[2];
	frame_buffer[idx_input].data[8] = lon.byte[3];

	//Checksum
	frame_buffer[idx_input].data[9] = gprs_checksum(
			(unsigned char *) frame_buffer[idx_input].data, 9);

	//Message len
	frame_buffer[idx_input].len = 9 + 1;

	//Update idx_input
	idx_input = (idx_input + 1) % GPRS_FRAME_BUFFER_SIZE;
	if (idx_input == idx_output)
		gprs_buffer_full = 1;

	//State machine
	switch (gprs_status) {
	case GPRS_IP_OK:
//Try tcp connection
		gprs_at_open_tcp_con(GPRS_SERVER_IP, GPRS_SERVER_PORT);
		gprs_status = GPRS_TCP_CONNECTING;
		gprs_send_mal = 0;
		break;
	case GPRS_TCP_CONNECTING:
	case GPRS_TCP_READY:
//Do nothing. Messages will be sent when we receive answer to connect or previous send commands
		break;
	default:
//Error. Something was wrong.
#ifdef GPRS_DEBUG
		printf("gprs_send error: unexpected status in state machine -> %d\n",
				gprs_status);
		fflush(stdout);
#endif
		gprs_status = GPRS_ERROR;
		gprs_configure = 0;
		idx_input = idx_output;
		return (-1);
		break;
	}

	return 0;

} //GPRS_SEND_POSITION END

/* GPRS_CHECKSUM: Calculate checksum to be added at the end of a frame. Add bytes and apply 2-complement. */
unsigned char gprs_checksum(unsigned char *data, short len) {
	char result = 0;
	short i = 0;

	for (i = 0; i < len; i++) {
		result += data[i];
	}

	//Two - complement
	result = ~result;
	result++;

	return result;
} // GPRS CHECKSUM END

/* IS_IP_V4: Returns 1 if input string is a well formated ip. Otherwise returns 0 */
int gprs_is_ip_v4(char* ip) {
	int num;
	int flag = 1;

	//Check length
	if ((strlen(ip) > 15) || (strlen(ip) < 7)) {
		return 0;
	}

	int counter = 0;
	char* p = strtok(ip, ".");

	while (p && flag) {
		num = atoi(p);

		if ((num >= 0) && (num <= 255) && (counter++ < 4)) {
			flag = 1;
			p = strtok(NULL, ".");
		} else {
			flag = 0;
			break;
		}
	}

	return flag && (counter == 4);
} // IS IP V4 END

/* GPRS_SEND_FRAME_IN_BUFFER: If there are messages in the gprs buffer to be sent, take the first one in the queue, and send it. */
void gprs_send_frame_in_buffer(void) {
	char at_cmd[255];

	if ((gprs_status != GPRS_TCP_READY) || (idx_input == idx_output)) {
		printf("Send frame in buffer error: Wrong gprs status %d\n",
				gprs_status);
		fflush(stdout);
		gprs_status = GPRS_ERROR;
		gprs_configure = 0;
		idx_input = idx_output;
		return;
	}

#if defined(SUPPORT_SLEEP)
	uart_write_buffer(fd_gprs, "a\r", 2);
	usleep(T_100MS);
#endif

	bzero(&at_cmd, 255);
	sprintf(at_cmd, "AT+CIPSEND=%d\r", frame_buffer[idx_output].len);

	int result = uart_write_buffer(fd_gprs, at_cmd, strlen(at_cmd));

	if (result < 0) {
#ifdef GPRS_DEBUG
		printf("GPRS: Error enviando comando AT+CIPSEND\n");
		fflush(stdout);
#endif
//Error in gprs
		if (gprs_critical_errors <= GPRS_CRITICAL_ERROR_LIMIT)
			gprs_critical_errors++;
		if (gprs_critical_errors <= GPRS_CRITICAL_ERROR_LIMIT) {
			gprs_status = GPRS_ERROR;
			gprs_configure = 0;
			idx_input = idx_output;
		}
		return;
	}
	//Wait for prompt
	usleep(T_100MS);

	//Send first message in buffer

	result = uart_write_buffer(fd_gprs, frame_buffer[idx_output].data,
			frame_buffer[idx_output].len);

	if (result < 0) {
#ifdef GPRS_DEBUG
		printf("Send frame in buffer error:\n");
		int i = 0;
		for (i = 0; i < frame_buffer[idx_output].len; i++) {
			printf("%02x ", frame_buffer[idx_output].data[i]);
		}
		printf("\n");
		fflush(stdout);
#endif
//Error in gprs
		if (gprs_critical_errors <= GPRS_CRITICAL_ERROR_LIMIT)
			gprs_critical_errors++;
		if (gprs_critical_errors <= GPRS_CRITICAL_ERROR_LIMIT) {
			gprs_status = GPRS_ERROR;
			gprs_configure = 0;
			idx_input = idx_output;
		}
		return;
	}

	//Update buffer index
	//NOTE: Output index will be updated when received send_ok
	//idx_output = (idx_output+1) % GPRS_FRAME_BUFFER_SIZE;

} // GPRS SEND FRAME IN BUFFER END

void gprs_get_network(void){

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
	gprs_process_msg();
}

void gprs_set_network(char* network){
 
	char gprs_msg[26];
	int i = 0;

	char *at_cmd = "AT+COPS=\r";
	sprintf(gprs_msg, "AT+COPS=4,2,\"%s\"\r", network);
	printf("%s\n", at_cmd); 
	int result = uart_write_buffer(fd_gprs, at_cmd, strlen(at_cmd));
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
		printf("ESPERANDO OK\r\n");
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
	result = uart_write_buffer(fd_gprs, gprs_msg, strlen(gprs_msg));

	at_cmd = "\x1a";
	printf("%s\n", at_cmd);
	result = uart_write_buffer(fd_gprs, at_cmd, strlen(at_cmd));
}


