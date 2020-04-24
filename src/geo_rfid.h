/*
 * geo_rfid.h
 *
 *  Created on: Nov 8, 2017
 *      Author:xabi1.1
 */

#ifndef GEO_RFID_H_
#define GEO_RFID_H_

// =======================================================================
//
//  DEFINES
//
// =======================================================================
#define RFID_DEBUG

#define WEB_PORT			5554 //UDP port for web module
#define PARAMS_PORT 		5555 //UDP port for parameters
#define CONTROL_PORT 		5556 //UDP port for control module
#define COMMUNICATIONS_PORT 5557 //UDP port for communications module
#define RFID_PORT			5558 //UDP port for rfid module
#define CO_KEEPALIVE 		0x10 //Code of operation for keepalive messaging
#define CO_RFID_REPORT		0x20 //Code of operation used by geo_rfid to repport a tag to communication module
#define ID_GEORFID			0x03
//#define RFID_READER_URI		"eapi:///dev/ttyUSB0"
#define RFID_READER_URI		"eapi:///dev/ttymxc3"
#define MAX_ERRORS			10

// =======================================================================
//
//  PROTOTYPES
//
// =======================================================================


#endif /* GEO_RFID_H_ */
