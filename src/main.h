/*
 * main.h
 *
 *  Created on: 17/04/2018
 *      Author: larraitz
 */


struct datos {
	char hora[9];
	uint8_t antena;
	int32_t RSSI;
	int numDatos;
};


struct tablaEPC {
	char EPC[24];
	struct datos datos[50];
	int numEPC;
};

struct recibidoEPC {
	char EPC[24];
};

struct datosBD {
	char EPC[24];
};

void addTag(char epc[], uint8_t ant, int32_t rssi);
void *funcionDelHilo (void *);
