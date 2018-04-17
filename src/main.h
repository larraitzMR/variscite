/*
 * main.h
 *
 *  Created on: 17/04/2018
 *      Author: larraitz
 */


struct datos {
	char hora[8];
	uint8_t antena;
	int32_t RSSI;
	int numDatos;
};


struct tablaEPC{
	char EPC[24];
	struct datos datos[50];
	int numEPC;
};

void addTagtoTable(struct tablaEPC *tabla, char epc[], uint8_t antena, int32_t rssi);
