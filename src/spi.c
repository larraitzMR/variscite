/*
 * Copyright (c) 2017 Fixso.
 * All rights reserved.
 *
 * Author      :xabi1.1
 * Description : This is a SPI library to be used in an embedded environment running linux
 * 				 Tested in ubuntu and debian compatible environments.
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdint.h>
#include <string.h>
#include <getopt.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <linux/types.h>
#include <linux/ioctl.h>
#include <linux/spi/spidev.h>
#include "spi.h"

//static const char *device = "/dev/spidev1.0";
static int spi_descriptor;
static unsigned short mode = 0; //Default mode
static unsigned short bits = 8; //Default bits per word
//static unsigned int speed = 5000000;

static uint16_t delay;
static int verbose;
//static uint32_t speed = 115200;

#define ARRAY_SIZE(a) (sizeof(a) / sizeof((a)[0]))

/*! ------------------------------------------------------------------------------------------------------------------
 * Function: openspi()
 *
 * Low level abstract function to open and initialise access to the SPI device.
 * returns 0 for success, or -1 for error
 */
int openspi(char *device, unsigned int speed) {
	int ret = 0;

	// Default SPI used is SPI0
	spi_descriptor = open(device, O_RDWR);
	if (spi_descriptor < 0)
		return (-1);

	/*
	 * spi mode
	 */
	ret = ioctl(spi_descriptor, SPI_IOC_WR_MODE, &mode);
	if (ret == -1)
		return (-1);

	ret = ioctl(spi_descriptor, SPI_IOC_RD_MODE, &mode);
	if (ret == -1)
		return (-1);

	/*
	 * bits per word
	 */
	ret = ioctl(spi_descriptor, SPI_IOC_WR_BITS_PER_WORD, &bits);
	if (ret == -1)
		return (-1);

	ret = ioctl(spi_descriptor, SPI_IOC_RD_BITS_PER_WORD, &bits);
	if (ret == -1)
		return (-1);

	/*
	 * max speed hz
	 */
	ret = ioctl(spi_descriptor, SPI_IOC_WR_MAX_SPEED_HZ, &speed);
	if (ret == -1)
		return (-1);

	ret = ioctl(spi_descriptor, SPI_IOC_RD_MAX_SPEED_HZ, &speed);
	if (ret == -1)
		return (-1);

	return 0;

} // end openspi()

/*! ------------------------------------------------------------------------------------------------------------------
 * Function: closespi()
 *
 * Low level abstract function to close the the SPI device.
 * returns 0 for success, or -1 for error
 */
int closespi(void) {
	close(spi_descriptor);

	return 0;

} // end closespi()


static void hex_dump(const void *src, size_t length, size_t line_size,
		char *prefix) {
	int i = 0;
	const unsigned char *address = src;
	const unsigned char *line = address;
	unsigned char c;

	printf("%s | ", prefix);
	while (length-- > 0) {
		printf("%02X ", *address++);
		if (!(++i % line_size) || (length == 0 && i % line_size)) {
			while (line < address) {
				c = *line++;
				printf("%c", (c < 33 || c == 255) ? 0x2E : c);
			}
			printf("\n");
			if (length > 0)
				printf("%s | ", prefix);
		}
	}
}

static void pabort(const char *s) {
	perror(s);
	abort();
}

void transfer(uint8_t const *tx, size_t lenTx, uint8_t const *rx, size_t lenRx) {

	struct spi_ioc_transfer	xfer[2];
	unsigned char		buf[32], *bp;
	int			status;

	//printf("do msg\n");

	memset(xfer, 0, sizeof xfer);

	xfer[0].tx_buf = (unsigned long)tx;
	xfer[0].len = lenTx;

	xfer[1].rx_buf = (unsigned long) rx;
	xfer[1].len = lenRx;

	status = ioctl(spi_descriptor, SPI_IOC_MESSAGE(2), xfer);
	if (status < 0) {
		perror("SPI_IOC_MESSAGE");
		return;
	}
}


void readSPI(uint8_t const *tx, size_t lenTx, uint8_t const *rx, size_t lenRx)
{

	struct spi_ioc_transfer	xfer[2];
	unsigned char		buf[32], *bp;
	int			status;

	//printf("do msg\n");

	memset(xfer, 0, sizeof xfer);

	xfer[0].tx_buf = (unsigned long)tx;
	xfer[0].len = lenTx;

	xfer[1].rx_buf = (unsigned long) rx;
	xfer[1].len = lenRx;

	status = ioctl(spi_descriptor, SPI_IOC_MESSAGE(2), xfer);
	if (status < 0) {
		perror("SPI_IOC_MESSAGE");
		return;
	}

	printf("response(%2d, %2d): ", lenRx, status);
	for (bp = rx; lenRx; lenRx--)
		printf(" %c", *bp++);
	printf("\n"); 
	hex_dump(buf, lenRx, 32, "RX");
}
