/*
 * Copyright (c) 2017 Fixso.
 * All rights reserved.
 *
 * Author      : Santi
 * Description : This is a SPI library to be used in an embedded environment running linux
 * 				 Tested in ubuntu and debian compatible environments.
 */

#ifndef _DECA_SPI_H_
#define _DECA_SPI_H_

#ifdef __cplusplus
extern "C" {
#endif



/*! ------------------------------------------------------------------------------------------------------------------
 * Function: openspi()
 *
 * Low level abstract function to open and initialise access to the SPI device.
 * returns 0 for success, or -1 for error
 */
int openspi(char *device, unsigned int speed) ;

/*! ------------------------------------------------------------------------------------------------------------------
 * Function: closespi()
 *
 * Low level abstract function to close the the SPI device.
 * returns 0 for success, or -1 for error
 */
int closespi(void) ;

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
					const unsigned char *bodyBuffer );



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
					 unsigned char *readBuffer );


#ifdef __cplusplus
}
#endif

#endif /* _DECA_SPI_H_ */
