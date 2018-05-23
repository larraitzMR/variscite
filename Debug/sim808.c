: Oct 2, 2017
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
int gprs_configure;	//1 if we enter into configuration mode

int fd_gprs; //File descriptor for gprs UART
char  gprs_imei[17];
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
void gprs_at_set_APN(char *apn,char *user, char *pass);
void gprs_at_network_up(void);
void gprs_at_get_ip(void);
void gprs_at_shut(void);
void gprs_at_close_con(void);

void gprs_at_open_tcp_con(char *ip, unsigned int port);
int gprs_is_ip_v4(char* ip);
unsigned char gprs_checksum(unsigned char *data, short len);
void gprs_send_frame_in_buffer(void);


// =======================================================================
//
//  LIBRARY FUNCTIONS
//
// =======================================================================

/* GPRS_INIT: This function turns on the module, configures the uart
 * 			  and initializes all necessary variables
 * 			  Returns uart file descriptor or gprs in case of error.
 */
int gprs_init(void){
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


	latitude.dato=0;
	longitude.dato=0;

	//Export and configure gpio for RESET pin
	if (gpio_config(SRES,GPIO_OUTPUT) < 0){
		printf("Error configuring RESET pin for SIM808\n");
		fflush(stdout);
		return(-1);
	}

	//Set reset pin high
	//gpio_write(SRES,1);

	//Export and configure gpio for POWERKEY pin
	if (gpio_config(POWERKEY,GPIO_OUTPUT) < 0){
		printf("Error configuring PWRKEY pin for SIM808\n");
		fflush(stdout);
		return(-1);
	}

	//Turn on module
	//gpio_write(POWERKEY,0);
	//sleep(1);
	//gpio_write(POWERKEY,1);

	//Configure uart
	fd_gprs = uart_open(UART_GPRS, 115200, "8N1");
	if (fd_gprs < 0){
		printf("Error configuring UART for SIM808\n");
		fflush(stdout);
		return(-1);
	}

	#ifdef GPRS_DEBUG
	printf("File descriptor in sim808: %d\n",fd_gprs);
	fflush(stdout);
	#endif

	//Enter low consumption mode: Do it from main

	return fd_gprs;
} // GPRS INIT END


/* GPRS_CLOSE: This function closes the gprs uart and turns off the module */
void gprs_close(void){
	//Close uart
	uart_close(fd_gprs);

	//Turn off module
	gpio_write(POWERKEY,0);
	sleep(1);
	gpio_write(POWERKEY,1);

} // GPRS CLOSE END

/* GPRS_CONFIGURE_AT: Configure GPRS via AT commands. Implements a state machine
 * 					  This function must be called twice, since first time, just
 * 					  basic tests are performed, and sleep configured if available.
 * 					  With basic configuration, only GPS is available for use.
 * */
void gprs_configure_AT(void){
	switch (gprs_status){
		case GPRS_OFF:
			gprs_config_mal=0;
			gprs_at_test();
			break;
		case GPRS_TEST_OK:
			gprs_config_mal=0;
			gprs_at_imei();
			break;
		case GPRS_IMEI_OK:
			gprs_config_mal=0;
			#if defined(SUPPORT_SLEEP)
			gprs_at_allow_sleep();
			#elif defined(PIN_REQUIRED)
			gprs_at_enter_pin();
			#else
			gprs_at_check_network();
			#endif
			break;
		case GPRS_SLEEP_ALLOWED_OK:
			gprs_config_mal=0;
			#if defined(PIN_REQUIRED)
			gprs_at_enter_pin();
			#else
			gprs_at_check_network();
			#endif
			break;
		case GPRS_PIN_OK:
			gprs_config_mal=0;
			gprs_at_check_network();
			break;
		case GPRS_NETWORK_OK:
			gprs_config_mal=0;
			gprs_at_set_context();
			break;
		case GPRS_CONTEXT_OK:
			gprs_config_mal=0;
			gprs_at_set_APN(GPRS_APN_NAME,GPRS_APN_USER,GPRS_APN_PASS);
			break;
		case GPRS_APN_READY:
			gprs_config_mal=0;
			gprs_at_network_up();
			break;
		case GPRS_NETWORK_UP:
			gprs_config_mal=0;
			gprs_at_get_ip();
			break;
		case GPRS_PDP_DEACT:
			gprs_config_mal=0;
			gprs_at_shut();
			break;
		default:
			usleep(T_100MS);
			if (gprs_config_mal <= GPRS_CONFIG_MAL_LIMIT) gprs_config_mal++;
			if (gprs_config_mal == GPRS_CONFIG_MAL_LIMIT){
				//Configuration process ended with critical errors
				gprs_status=GPRS_ERROR;
				gprs_configure=0;
				printf("GPRS: Error limit during configuration reached\n");
				fflush(stdout);
			}
			break;
	}
}// END GPRS CONFIGURE AT


/* GPRS_PROCESS_MSG: Parse responses to AT commands received from GPRS module.
 * 					 Changes state machine status as a result.
 */
void gprs_process_msg(void){
	char gprs_msg[255];
	char separators[4] = "\r\n";
	char separators2[6] = ",:\r\n ";
	//char separators3[9] = "+,:/\"\r\n ";
	char *temp;
	//char temp2[128];
	char *temp3;

	//Clean buffer
	bzero(&gprs_msg,sizeof(gprs_msg));

	//Read (all) from uart
	while (uart_read(fd_gprs, gprs_msg, sizeof(gprs_msg)) > 0){
		//TODO: If we receive a string starting with * (0x2a) we can consider it is a message from our server


		//Separate lines by NL and CR
		temp = strtok(gprs_msg,separators);
		//while (temp != NULL){
		if (temp != NULL){

			#ifdef GPRS_DEBUG
			if (strlen(temp) > 1){
				printf("%s\n",temp);
				fflush(stdout);
			}
			#endif

			if (temp[0] == '>'){
				//Signal for us to send tcp message
			}else if (strncmp("86", temp, 2) == 0){
				bzero(&gprs_imei, sizeof(gprs_imei));
				//Recibido imei: Sacamos numero de serie
				strncpy(gprs_imei,temp,strlen(temp));
				if (gprs_prev_status == GPRS_TEST_OK){
					gprs_status = GPRS_IMEI_OK;
				 }
			}else if (strncmp("+CREG:",temp,6) == 0){
				if ((temp[9] == '1') || (temp[9] == '5')){
					//Registrado o registrado en roaming
					gprs_status = GPRS_NETWORK_OK;
				}else if (temp[9] == 3){
					//Register denied
					if (gprs_critical_errors <= GPRS_CRITICAL_ERROR_LIMIT) gprs_critical_errors++;
					if (gprs_critical_errors == GPRS_CRITICAL_ERROR_LIMIT){
						gprs_status=GPRS_ERROR;
						gprs_configure=0;
						printf("Max GPRS critical errors reached. Recommended to use another network interface.\n");
						fflush(stdout);
						//Messages in buffer will be lost
						idx_input=idx_output;
					}else{
						//Not registered, keep waiting
						gprs_status = gprs_prev_status;
					}
				}else{
					//Not registered, keep waiting
					gprs_status = gprs_prev_status;
				}
			}else if (strncmp("OK", temp, 2) == 0){
				//If we are in config mode, move to next state
				if ((gprs_configure) && (gprs_status == GPRS_IDLE)){
					switch (gprs_prev_status){
						case GPRS_OFF:
							gprs_status=GPRS_TEST_OK;
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
							gprs_status=GPRS_NETWORK_OK;
							#endif
							break;
						case GPRS_SLEEP_ALLOWED_OK:
							#if defined(PIN_REQUIRED)
							gprs_status=GPRS_PIN_OK;
							#else
							gprs_status=GPRS_NETWORK_OK;
							#endif
							break;
						case GPRS_PIN_OK:
							gprs_status=GPRS_NETWORK_OK;
							break;
						case GPRS_NETWORK_OK:
							gprs_status=GPRS_CONTEXT_OK;
							break;
						case GPRS_CONTEXT_OK:
							gprs_status=GPRS_APN_READY;
							break;
						case GPRS_APN_READY:
							gprs_status=GPRS_NETWORK_UP;
							break;
						//case GPRS_NETWORK_UP:
						//	gprs_status=GPRS_IP_OK;
						//	break;
						default:
							break;
					} // End switch
				}

				//TODO: OK response to other commands, different from configuration
			}else if (gprs_is_ip_v4(temp)){
				//Local ip received
				gprs_critical_errors=0;
				if ((gprs_configure) && (gprs_status == GPRS_IDLE) && (gprs_prev_status == GPRS_NETWORK_UP)){
					gprs_status = GPRS_IP_OK;
					gprs_configure = 0;
				}
			}else if (strncmp("CONNECT OK", temp, 10) == 0){
				gprs_critical_errors=0;
				gprs_status=GPRS_TCP_READY;
				if (idx_output != idx_input){
					//Send message from buffer
					gprs_send_frame_in_buffer();
				}
			}else if (strncmp ("CLOSE OK", temp, 8) == 0){
				gprs_status=GPRS_IP_OK;
				idx_input=idx_output; //Just to be sure that everything is ok
			}else if (strncmp ("CLOSED", temp, 6) == 0){
				gprs_status=GPRS_IP_OK;
				//In this case, some messages can be lost
				if (idx_input != idx_output){
					printf("TCP connection closed unexpectedly: Some frames will be lost\n");
					fflush(stdout);
					idx_input=idx_output;
				}
			}else if (strncmp ("SEND OK", temp, 7) == 0){
				gprs_critical_errors=0;
				//Update buffer index
				idx_output = (idx_output+1) % GPRS_FRAME_BUFFER_SIZE;
				gprs_send_mal=0;
				if (idx_input != idx_output){
					//If there is more messages in buffer, send another one
					gprs_send_frame_in_buffer();
				}else{
					//In other case, close connection
					gprs_at_close_con();
				}
			}else if (strncmp ("SEND FAIL", temp, 9) == 0){
				//Don't update buffer index
				gprs_send_mal++;
				if (gprs_send_mal == GPRS_SEND_MAL_LIMIT){
					//Status to PDP DEACT, so we are forced to reset connection
					if (gprs_status != GPRS_ERROR){
						gprs_status = GPRS_PDP_DEACT;
						gprs_configure = 1;
					}

					//Messages will be lost
					printf("Restarting network connection: Some frames will be lost\n");
					fflush(stdout);
					idx_input=idx_output;
				}
			}else if (strncmp ("SHUT OK", temp, 7) == 0){
				if (gprs_status != GPRS_ERROR){
					gprs_status = GPRS_NETWORK_OK;
					gprs_configure = 1;
				}
			}else if ((strncmp("+CGNSINF:", temp, 9) == 0) || (strncmp("+UGNSINF:", temp, 9) == 0)){
				//Format: +CGNSINF: <GNSSS run status>, <fix status>, <UTC Date & time>, <latitude>, <longitude> .....
				temp3 = strtok(temp,separators2); //+CGNSINF
				temp3 = strtok(NULL,separators2); //run status
				if (temp3[0] == '1'){
					temp3 = strtok(NULL,separators2); //fix status
					if (temp3[0] == '1'){
						//Get date and position
						gps_status=GPS_OK;
						temp3 = strtok(NULL,separators2); //utc date & time
						bzero(&gps_utc_time, sizeof(gps_utc_time));
						strncpy(gps_utc_time,temp3,18);
						temp3 = strtok(NULL,separators2); //latitude
						latitude.dato = atof(temp3);
						temp3 = strtok(NULL,separators2); //longitude
						longitude.dato = atof(temp3);
					}else{
						gps_status=GPS_NO_COVERAGE;
					}
				}else{
					gps_status=GPS_NO_COVERAGE;
				}
			}else if (strncmp("+PDP: DEACT", temp, 11) == 0){
				if ((gprs_status != GPRS_ERROR) && (gprs_status != GPRS_LOW_POWER)){
					gprs_status = GPRS_PDP_DEACT;
					gprs_configure = 1;
				}

			}else if ((strncmp("ERROR", temp, 5) == 0) || (strncmp("+CME ERROR", temp, 10) == 0)){
				if (gprs_critical_errors <= GPRS_CRITICAL_ERROR_LIMIT) gprs_critical_errors++;
				if (gprs_critical_errors == GPRS_CRITICAL_ERROR_LIMIT){
					gprs_status=GPRS_ERROR;
					gprs_configure=0;
					printf("Max GPRS critical errors reached. Recommended to use another network interface.\n");
					fflush(stdout);
					//Messages in buffer will be lost
					idx_input=idx_output;
				}else if (gprs_configure){
					//If we don't reach max. allowed errors, repeat last command.
					if (gprs_status < GPRS_CONTEXT_OK){
						gprs_status=gprs_prev_status;
					}else{
						gprs_status = GPRS_PDP_DEACT;
					}
				}

			}else{
				//Do nothing with other messages
				#ifdef GPRS_DEBUG
				if (strlen(temp) > 1){
					printf("NO PARSEADO: *%s*\n",temp);
					fflush(stdout);
				}
				#endif
			}


			//Get next token
			//temp = strtok(NULL,separators);
		}

		//Clean buffer
		bzero(&gprs_msg,sizeof(gprs_msg));
	}
} // GPRS PROCESS MSG END


/* GPRS AT TEST: Send AT command for testing communication with module
 * 				 As a result, module will adjust auto-baudrate
 */
void gprs_at_test(void){
	char *at_cmd = "AT\r\n";

	int result = uart_write_buffer(fd_gprs, at_cmd, strlen(at_cmd));

	//usleep(T_100MS);

	//result = uart_write_buffer(fd_gprs, at_cmd, strlen(at_cmd));

	#ifdef GPRS_DEBUG
		printf("%s\n",at_cmd);
		if (result < 0){
			printf("GPRS: Error enviando comando AT\n");
		}
		fflush(stdout);
	#endif

	gprs_status = GPRS_IDLE;
	gprs_prev_status = GPRS_OFF;
} // GPRS AT TEST END


/* GPRS AT IMEI: Send AT command to request imei */
void gprs_at_imei(void){
	char *at_cmd = "AT+CGSN\r";

	int result = uart_write_buffer(fd_gprs, at_cmd, strlen(at_cmd));

	#ifdef GPRS_DEBUG
		printf("%s\n",at_cmd);
		if (result < 0){
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
void gprs_at_minimum_functionality(void){
	char *at_cmd = "AT+CFUN=0\r";

	int result = uart_write_buffer(fd_gprs, at_cmd, strlen(at_cmd));

	#ifdef GPRS_DEBUG
		printf("%s\n",at_cmd);
		if (result < 0){
			printf("GPRS: Error enviando comando AT+CFUN\n");
		}
		fflush(stdout);
	#endif

	gprs_status=GPRS_LOW_POWER;

} // GPRS AT MINIMUM FUNCTIONALITY END


/* GPRS_AT_FULL_FUNCTIONALITY: This module performs a hard reset, resulting in a GPRS module waking up
 * 							   in normal functionality mode. GPRS status and GPS status are reset to PWR_OFF.
 * 							   GPRS enters configuration mode.
 */
void gprs_at_full_functionality(void){
	char *at_cmd = "AT+CFUN=1,1\r";

	int result = uart_write_buffer(fd_gprs, at_cmd, strlen(at_cmd));

	#ifdef GPRS_DEBUG
		printf("%s\n",at_cmd);
		if (result < 0){
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
void gprs_at_allow_sleep(void){
	char *at_cmd = "AT+CSCLK=2\r";

	int result = uart_write_buffer(fd_gprs, at_cmd, strlen(at_cmd));

	#ifdef GPRS_DEBUG
		printf("%s\n",at_cmd);
		if (result < 0){
			printf("GPRS: Error enviando comando AT+CGSN\n");
		}
		fflush(stdout);
	#endif

	gprs_status = GPRS_IDLE;
	gprs_prev_status = GPRS_IMEI_OK;
} // GPRS AT ALLOW SLEEP END


/* GPRS_AT_ENTER_PIN: Enter pin for sim card */
void gprs_at_enter_pin(void){
	char at_cmd[255];

	//Wake up module
	#if defined(SUPPORT_SLEEP)
	uart_write_buffer(fd_gprs, "a\r", 2);
	usleep(T_100MS);
	#endif

	bzero(&at_cmd,255);
	sprintf(at_cmd, "AT+CPIN=%s\r","3870");

	int result = uart_write_buffer(fd_gprs, at_cmd, strlen(at_cmd));

	#ifdef GPRS_DEBUG
		printf("%s\n",at_cmd);
		if (result < 0){
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
void gprs_at_check_network(void){
	char *at_cmd = "AT+CREG?\r";

	//Wake up module
	#if defined(SUPPORT_SLEEP)
	uart_write_buffer(fd_gprs, "a\r", 2);
	usleep(T_100MS);
	#endif

	int result = uart_write_buffer(fd_gprs, at_cmd, strlen(at_cmd));

	#ifdef GPRS_DEBUG
		printf("%s\n",at_cmd);
		if (result < 0){
			printf("GPRS: Error enviando comando AT+CREG\n");
		}
		fflush(stdout);
	#endif

	gprs_prev_status = gprs_status;
	gprs_status = GPRS_IDLE;

} // GPRS AT CHECK NETWORK END


/* GPRS_AT_SET_CONTEXT: Send at command to set GPRS context */
void gprs_at_set_context(void){
	char *at_cmd = "AT+CGATT=1\r";

	//Wake up module
	#if defined(SUPPORT_SLEEP)
	uart_write_buffer(fd_gprs, "a\r", 2);
	usleep(T_100MS);
	#endif

	int result = uart_write_buffer(fd_gprs, at_cmd, strlen(at_cmd));

	#ifdef GPRS_DEBUG
		printf("%s\n",at_cmd);
		if (result < 0){
			printf("GPRS: Error enviando comando AT+CGATT\n");
		}
		fflush(stdout);
	#endif

	gprs_status = GPRS_IDLE;
	gprs_prev_status = GPRS_NETWORK_OK;
} // GPRS AT SET CONTEXT END


/* GPRS_AT_SET_APN: Send at command to configure apn information */
void gprs_at_set_APN(char *apn,char *user, char *pass){
	char at_cmd[255];

	#if defined(SUPPORT_SLEEP)
	uart_write_buffer(fd_gprs, "a\r", 2);
	usleep(T_100MS);
	#endif

	bzero(&at_cmd,255);
	sprintf(at_cmd, "AT+CSTT=\"%s\",\"%s\",\"%s\"\r",apn,user,pass);

	int result = uart_write_buffer(fd_gprs, at_cmd, strlen(at_cmd));

	#ifdef GPRS_DEBUG
		printf("%s\n",at_cmd);
		if (result < 0){
			printf("GPRS: Error enviando comando AT+CSTT\n");
		}
		fflush(stdout);
	#endif

	gprs_status = GPRS_IDLE;
	gprs_prev_status = GPRS_CONTEXT_OK;
} // GPRS AT SET APN END


/* GPRS_AT_NETWORK_UP: Brings up the gprs wireless connection with the apn previously configured */
void gprs_at_network_up(void){
	char *at_cmd = "AT+CIICR\r";

	#if defined(SUPPORT_SLEEP)
	uart_write_buffer(fd_gprs, "a\r", 2);
	usleep(T_100MS);
	#endif

	int result = uart_write_buffer(fd_gprs, at_cmd, strlen(at_cmd));

	#ifdef GPRS_DEBUG
		printf("%s\n",at_cmd);
		if (result < 0){
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
void gprs_at_get_ip(void){
	char *at_cmd = "AT+CIFSR\r";

	#if defined(SUPPORT_SLEEP)
	uart_write_buffer(fd_gprs, "a\r", 2);
	usleep(T_100MS);
	#endif

	int result = uart_write_buffer(fd_gprs, at_cmd, strlen(at_cmd));

	#ifdef GPRS_DEBUG
		printf("%s\n",at_cmd);
		if (result < 0){
			printf("GPRS: Error enviando comando AT+CIFSR\n");
		}
		fflush(stdout);
	#endif

	gprs_status = GPRS_IDLE;
	gprs_prev_status = GPRS_NETWORK_UP;
} // GPRS AT GET IP END
