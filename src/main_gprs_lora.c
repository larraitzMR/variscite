#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <time.h>
#include <linux/spi/spidev.h>
#include "spi.h"
#include "lmic/lmic.h"
#include "sim808.h"
#include "network.h"
#include "geo_rfid.h"
#include "reader_params.h"
#include "main.h"
#include "spi.h"

static int coordenadas;

int main(void) {


	//Init GPRS
	int fd_sim808 = 0;
	int result = 0;
	fd_set rfds;
	struct timeval tv;

	printf("Probando GPRS: \n");
	printf("============== \n");
	fflush(stdout);

	//Iniciamos gprs
	fd_sim808 = gprs_init();
	if (fd_sim808 <= 0) {
		printf("Error arrancando gprs\n");
		fflush(stdout);
		return (-1);
	}
	printf("fd_sim808: %d\n", fd_sim808);
	fflush(stdout);


	gps_init();

	gps_get_location();

	int fd_stmf4; 
	static const char *device = "/dev/spidev2.0";

    char buffRX[5];

	//Configure spi
	fd_stmf4 = openspi(device, 115200);
	if (fd_stmf4 < 0) {
	        printf("Error configuring SPI for variscite\n");
	        fflush(stdout);
	        return (-1);
	}

	char *coor;

	while(1) {
		//gprs_get_network();
		//sleep(1);
		gprs_process_msg();
		coor = gps_send_location_spi();
		printf("coor: %s\n", coor);
		transfer(coor, 20, 0, 0);
        sleep(2);
        readSPI(0,0, buffRX,20);
//              printf("Buff: %s\n", buffRX);
        sleep(2);
        memset(buffRX, 0, sizeof(buffRX));
		
	}
}