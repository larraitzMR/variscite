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
#include <fcntl.h>
#include <sys/ioctl.h>
#include <linux/types.h>
#include <linux/spi/spidev.h>
#include "spi.h"



//static const char *device = "/dev/spidev1.0";
static int spi_descriptor;
static unsigned short mode = 0; //Default mode
static unsigned short bits = 8; //Default bits per word
//static unsigned int speed = 5000000;




/*! ------------------------------------------------------------------------------------------------------------------
 * Function: openspi()
 *
 * Low level abstract function to open and initialise access to the SPI device.
 * returns 0 for success, or -1 for error
 */
int openspi(char *device, unsigned int speed){
	int ret = 0;

	// Default SPI used is SPI0
	spi_descriptor = open(device, O_RDWR);
	if (spi_descriptor < 0) return(-1);

	/*
	 * spi mode
	 */
	ret = ioctl(spi_descriptor, SPI_IOC_WR_MODE, &mode);
	if (ret == -1) return (-1);

	ret = ioctl(spi_descriptor, SPI_IOC_RD_MODE, &mode);
	if (ret == -1) return (-1);

	/*
	 * bits per word
	 */
	ret = ioctl(spi_descriptor, SPI_IOC_WR_BITS_PER_WORD, &bits);
	if (ret == -1) return (-1);

	ret = ioctl(spi_descriptor, SPI_IOC_RD_BITS_PER_WORD, &bits);
	if (ret == -1) return (-1);

	/*
	 * max speed hz
	 */
	ret = ioctl(spi_descriptor, SPI_IOC_WR_MAX_SPEED_HZ, &speed);
	if (ret == -1) return (-1);

	ret = ioctl(spi_descriptor, SPI_IOC_RD_MAX_SPEED_HZ, &speed);
	if (ret == -1) return (-1);

	return 0;

} // end openspi()

/*! ------------------------------------------------------------------------------------------------------------------
 * Function: closespi()
 *
 * Low level abstract function to close the the SPI device.
 * returns 0 for success, or -1 for error
 */
int closespi(void)
{
	close(spi_descriptor);

	return 0;

} // end closespi()

/*! ------------------------------------------------------------------------------------------------------------------
 * Function: writetospi()
 *
 * Low level abstract function to write to the SPI
 * Takes two separate byte buffers for write header and write data
 * returns 0 for success, or -1 for error
 */
int writetospi( unsigned short headerLength,
			   	    const unsigned char *headerBuffer,
					unsigned int bodylength,
					const unsigned char *bodyBuffer ){
	int ret = 0;

    struct spi_ioc_transfer tr[2] = {{0}, };


    //header tx buffer
    tr[0].tx_buf = (unsigned long)headerBuffer;
    tr[0].rx_buf = (unsigned long)NULL;
    tr[0].len = headerLength;

    //body tx buffer
    tr[1].tx_buf = (unsigned long)bodyBuffer;
    tr[1].rx_buf = (unsigned long)NULL;
    tr[1].len = bodylength;

    //Send header and body
    ret = ioctl(spi_descriptor, SPI_IOC_MESSAGE(2), &tr);
    if (ret < 1) return (-1);

    return 0;
} // end writetospi()


/*! ------------------------------------------------------------------------------------------------------------------
 * Function: readfromspi()
 *
 * Low level abstract function to read from the SPI
 * Takes two separate byte buffers for write header and read data
 * returns the offset into read buffer where first byte of read data may be found,
 * or returns -1 if there was an error
 */
int readfromspi( unsigned short	headerLength,
			    	 const unsigned char *headerBuffer,
					 unsigned int readlength,
					 unsigned char *readBuffer ){
	int ret =0;

    struct spi_ioc_transfer tr[2] = {{0}, };

    //header tx buffer
    tr[0].tx_buf = (unsigned long)headerBuffer;
    tr[0].rx_buf = (unsigned long)NULL;
    tr[0].len = headerLength;

    //Receive buffer
    tr[1].tx_buf = (unsigned long)NULL;
    tr[1].rx_buf = (unsigned long)readBuffer;
    tr[1].len = readlength;

    ret = ioctl(spi_descriptor, SPI_IOC_MESSAGE(2), &tr);


    if (ret < 1) return (-1);

    return 0;
} // end readfromspi()

static const char *device = "/dev/spidev0.0";
static char *input_file;
static char *output_file;
static uint16_t delay;
static int verbose;
static uint32_t speed = 115200;

#define ARRAY_SIZE(a) (sizeof(a) / sizeof((a)[0]))

static void pabort(const char *s) {
	perror(s);
	abort();
}

uint8_t default_tx[] = { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x40, 0x00, 0x00,
		0x00, 0x00, 0x95, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
		0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xF0, 0x0D, };

uint8_t default_rx[ARRAY_SIZE(default_tx)] = { 0, };
char *input_tx;

static void hex_dump(const void *src, size_t length, size_t line_size,
		char *prefix) {
	int i = 0;
	const unsigned char *address = src;
	const unsigned char *line = address;
	unsigned char c;

	printf("%s | ", prefix);
	while (length-- > 0) {
//		printf("%02X ", *address++);
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

void transfer(uint8_t const *tx, uint8_t const *rx, size_t len) {
	int ret;
	int out_fd;

	printf("transfer\n");
	struct spi_ioc_transfer tr = { .tx_buf = (unsigned long) tx, .rx_buf =
			(unsigned long) rx, .len = len, .delay_usecs = delay, .speed_hz =
			speed, .bits_per_word = bits, };

	if (mode & SPI_TX_QUAD)
		tr.tx_nbits = 4;
	else if (mode & SPI_TX_DUAL)
		tr.tx_nbits = 2;
	if (mode & SPI_RX_QUAD)
		tr.rx_nbits = 4;
	else if (mode & SPI_RX_DUAL)
		tr.rx_nbits = 2;
	if (!(mode & SPI_LOOP)) {
		if (mode & (SPI_TX_QUAD | SPI_TX_DUAL))
			tr.rx_buf = 0;
		else if (mode & (SPI_RX_QUAD | SPI_RX_DUAL))
			tr.tx_buf = 0;
	}

	ret = ioctl(spi_descriptor, SPI_IOC_MESSAGE(1), &tr);
	if (ret < 1)
		pabort("can't send spi message");

	if (verbose)
		hex_dump(tx, len, 32, "TX");

//	if (output_file) {
//		out_fd = open(output_file, O_WRONLY | O_CREAT | O_TRUNC, 0666);
//		if (out_fd < 0)
//			pabort("could not open output file");
//
//		ret = write(out_fd, rx, len);
//		if (ret != len)
//			pabort("not all bytes written to output file");
//
//		close(out_fd);
//	}

//	if (verbose || !output_file)
//		hex_dump(rx, len, 32, "RX");
}
