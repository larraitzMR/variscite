/*
 * main.h
 *
 *  Created on: 17/04/2018
 *      Author: larraitz
 */


struct datos {
	int numDatos;
	char hora[8];
	uint8_t antena;
	int32_t RSSI;
};


struct tablaEPC{
	int numEPC;
	char EPC[24];
	struct datos datos[50];
};

void addTag(char epc[], uint8_t ant, int32_t rssi);

