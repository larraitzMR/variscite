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

void get_MaxMinPower(TMR_Reader *rp, uint16_t *powerMin, uint16_t *powerMax);
void getoneparam(TMR_Reader *rp, TMR_Param key);
void **getParam(TMR_Reader *rp, TMR_Param key);
