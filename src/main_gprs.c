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

	gprs_get_network();
	gprs_set_network("21407");

	//gprs_send_SMS("+34649103025", "Prueba");

	while(1) {
		//gprs_get_network();
		//sleep(1);
		gprs_process_msg();
	}
}