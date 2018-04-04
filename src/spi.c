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

