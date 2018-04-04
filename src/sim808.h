/*
 * sim808.h
 *
 *  Created on: Oct 2, 2017
 *      Author:xabi1.1
 */

#ifndef SIM808_H_
#define SIM808_H_

#include "uart.h"
#include "gpio.h"

// =======================================================================
//
//  DEFINES
//
// =======================================================================
#define GPRS_DEBUG
//#define PIN_REQUIRED
//#define SUPPORT_SLEEP

//#define UART_GPRS 		"/dev/ttymxc2"
#define UART_GPRS 			"/dev/ttyUSB0"
#define GPRS_APN_NAME		"internet.easym2m.eu"
//#define GPRS_APN_NAME		"NXT17.NET"
#define GPRS_APN_USER		""
#define GPRS_APN_PASS		""
#define GPRS_SIM_PIN		"1234"
#define GPRS_SERVER_IP		"5.135.120.65"
#define GPRS_SERVER_PORT	80

#define T_100MS	  100000 //For sleep

#define GPRS_CONFIG_MAL_LIMIT   	200   //times without GPRS answer
#define GPRS_SEND_MAL_LIMIT			10	  //number of consecutive times that sending a tcp message can finish with error
#define GPRS_CRITICAL_ERROR_LIMIT	10	  //number of critical errors allowed to decide that gprs is not working properly
#define GPRS_MAX_FRAME_SIZE 		255   //Max number of bytes per gprs frame
#define GPRS_FRAME_BUFFER_SIZE  	10 	  //Max number of messages in queue to be sent


//GPIOs
#define POWERKEY	8
#define SRES		7


//CODIGOS DE OPERACION



//GPRS STATUS DEFINITIONS
typedef enum {
    GPRS_OFF = 1,           /** GPRS POWERED OFF */
    GPRS_ERROR,				/** GPRS has been used before, but final status was ERROR. Must be reseted before use again */
    GPRS_LOW_POWER,			/** GPRS entered minimum functionality mode. Module must be reset and configured after that */
    GPRS_PDP_DEACT,			/** Received PDP DEACT during configuration, context must be reinitialized */
    GPRS_IDLE,              /** Idle status. No actions required while in this state */
    GPRS_TEST_OK,           /** Received response to single AT command */
    GPRS_IMEI_OK,			/** Received imei from GPRS module. It could be useful to use as some kind of serial number */
    GPRS_SLEEP_ALLOWED_OK,  /** Received OK response to sleep mode allow command */
    GPRS_PIN_OK,			/** Received OK after introducing PIN for SIM card */
    GPRS_NETWORK_OK,        /** Received OK response to gsm network connection command */
    GPRS_CONTEXT_OK,        /** Received OK response to set context command */
    GPRS_APN_READY,         /** Received OK response to configure apn command */
    GPRS_NETWORK_UP,        /** Received OK response to CIICR command */
    GPRS_IP_OK,				/** Received OK response to ip request command (CIFSR)*/
    GPRS_TCP_CONNECTING,    /** Trying to stablish TCP connection */
    GPRS_TCP_READY,         /** UDP CONNECTED, ready to transmit packets */
} GPRS_STATUS;

//GPS STATUS DEFINITIONS
typedef enum {
	GPS_OFF = 1,			/** GPS powered OFF */
	GPS_NO_COVERAGE,		/** GPS ON, without satellite reception */
	GPS_OK,					/** GPS ON, with good satellite reception */
} GPS_STATUS;


typedef union {
    float dato;
    unsigned char byte[4];
} t_position;


typedef struct {
	char data[GPRS_MAX_FRAME_SIZE];
	unsigned short len;
} t_frame;

// =======================================================================
//
//  PROTOTYPES
//
// =======================================================================
int gprs_init(void);
void gprs_close(void);
void gprs_configure_AT(void);
void gprs_process_msg(void);
void gprs_at_test(void);
void gprs_at_check_network(void);
void gprs_at_set_context(void);
void gprs_at_imei(void); //??
void gprs_at_minimum_functionality(void); //Minimum functionality. Radio off. GPS can be used.
void gprs_at_full_functionality(void); //Launches full configuration
unsigned short gprs_get_status(void);
void gps_power_on(int auto_report);
void gps_power_off(void);
void gps_at_get_data(void);
t_position gps_get_latitude(void);
t_position gps_get_longitude(void);
unsigned short gps_get_status(void);
int gprs_get_config(void);
//void gprs_set_config(void);
int gprs_check_error(void);
int gprs_send_position( t_position lat, t_position lon);





#endif /* SIM808_H_ */
