#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include "spi.h"


// =======================================================================
//
//  GLOBAL VARIABLES
//
// =======================================================================

int fd_stmf4; //File descriptor for gprs UART
static const char *device = "/dev/spidev0.0";

int main(void) {


        char buffRX[5];

        //Configure uart
        fd_stmf4 = openspi(device, 115200);
        if (fd_stmf4 < 0) {
                printf("Error configuring SPI for variscite\n");
                fflush(stdout);
                return (-1);
        }

        while(1) {
                transfer("LARRAITZ", 8, 0, 0);
                sleep(2);
                readSPI(0,0, buffRX,5);
//              printf("Buff: %s\n", buffRX);
                sleep(2);

        }
}

