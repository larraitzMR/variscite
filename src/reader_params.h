/*
 * reader_params.h
 *
 *  Created on: March 22, 2018
 *      Author: Larraitz
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include "m6e/tm_reader.h"
#include "network.h"
#include "geo_rfid.h"

void printU8List(TMR_uint8List *list, int *ports);
void getAntennaList(char *lchar, TMR_uint8List *list);
void getConnectedAntennaPorts(TMR_Reader *rp, int *puertos);
void getAntennaPorts(TMR_Reader *rp, int *p);
void getSelectedAntennas(TMR_Reader *rp, int *p);
void getReaderInfo(TMR_Reader *rp, TMR_Param key, char *inf);
void **getParam(TMR_Reader *rp, TMR_Param key);
