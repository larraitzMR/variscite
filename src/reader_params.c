/*
 * reader_params.c
 *
 *  Created on: March 22, 2018
 *      Author: Larraitz
 */

#include <inttypes.h>
#include "m6e/tmr_utils.h"
#include "reader_params.h"

void printU8List(TMR_uint8List *list, int *ports) {
	int i;

	putchar('[');
	for (i = 0; i < list->len && i < list->max; i++) {
		printf("%u%s", list->list[i], ((i + 1) == list->len) ? "" : ",");
		ports[i] = list->list[i];
	}
	if (list->len > list->max) {
		printf("...");
	}
	putchar(']');
}

void getAntennaList(char *lchar, TMR_uint8List *list)
{
	int i= 0;;
	char* p;
	list->len = 0;
	list->max = 4;
	list->list = malloc(sizeof(list->list[0]) * list->max);
	putchar('[');
	for (p = strtok(lchar + 1, " "); p; p = strtok( NULL, " ")) {
		int a = atoi(p);
		list->list[list->len] = a;
		list->len++;
		printf("%u%s", list->list[i], ((i + 1) == list->len) ? "" : ",");
		i++;
	}
	putchar(']');
	printf("\n");
}


void getConnectedAntennaPorts(TMR_Reader *rp, int *puertos) {
	TMR_Status ret;
	TMR_uint8List value;
	uint8_t valueList[64];
	value.max = numberof(valueList);
	value.list = valueList;
	ret = TMR_paramGet(rp, TMR_PARAM_ANTENNA_CONNECTEDPORTLIST, &value);
	printU8List(&value, puertos);
}

void getAntennaPorts(TMR_Reader *rp, int *p) {
	TMR_Status ret;
	TMR_uint8List value;
	uint8_t valueList[64];
	value.max = numberof(valueList);
	value.list = valueList;
	ret = TMR_paramGet(rp, TMR_PARAM_ANTENNA_PORTLIST, &value);
	printU8List(&value, p);
	for (int var = 0; var < 4; ++var) {
		printf("Puertos: %d ", p[var]);
	}
	printf("\n");
}

void getSelectedAntennas(TMR_Reader *rp, int *p) {
	TMR_Status ret;
	//TMR_uint8List value;
	uint8_t valueList[64];
	TMR_ReadPlan plan;
	ret = TMR_paramGet(rp, TMR_PARAM_READ_PLAN, &plan);
	printU8List(&plan.u.simple.antennas, p);
	for (int var = 0; var < 4; ++var) {
		printf("Puertos: %d ", p[var]);
	}
	printf("\n");
}

void getReaderInfo(TMR_Reader *rp, TMR_Param key, char *inf) {
	TMR_String info;
	TMR_Status ret;
	//char inf[1];
    char stringValue[256];
    info.max = sizeof(stringValue);
    info.value = stringValue;

	ret = TMR_paramGet(rp, key, &info);
	if (TMR_SUCCESS != ret) {
		printf("Error getting Reader Info: %s\n", ret);
	}
	strcpy(inf, info.value);
}

void **getParam(TMR_Reader *rp, TMR_Param key) {
	TMR_Status ret;

	if (TMR_PARAM_RADIO_POWERMAX == key) {
		uint16_t value;
		ret = TMR_paramGet(rp, key, &value);
		if (TMR_SUCCESS != ret) {
			goto out;
		}
		//printf("%u\n", value);
		return value;
	} else if (TMR_PARAM_RADIO_POWERMIN == key) {
		uint16_t value;
		ret = TMR_paramGet(rp, key, &value);
		if (TMR_SUCCESS != ret) {
			goto out;
		}
		//printf("%u\n", value);
		return value;
	} else if (TMR_PARAM_ANTENNA_CHECKPORT == key) {
		bool value;
		uint16_t v;
		ret = TMR_paramGet(rp, key, &value);
		if (TMR_SUCCESS != ret) {
			goto out;
		}
		printf("%s\n", value ? "true" : "false");
		v = value;
		//printf("V: %d\n", v);
		return v;
	} else if (TMR_PARAM_RADIO_READPOWER == key) {
		uint32_t value;
		ret = TMR_paramGet(rp, key, &value);
		if (TMR_SUCCESS != ret) {
			goto out;
		}
		printf("Power: %u\n", value);
		return value;
	}
	out: if (TMR_SUCCESS != ret) {
		printf("Error retrieving value: %s\n", TMR_strerr(rp, ret));
	}
}
