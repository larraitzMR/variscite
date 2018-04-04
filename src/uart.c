/*
 * uart.c
 *
 *  Created on: Oct 4, 2017
 *      Author:xabi1.1
 */

#include "uart.h"



// =======================================================================
//
//  LIBRARY FUNCTIONS
//
// =======================================================================


/* UART_OPEN: Opens and configure a given serial device.
 * 			  Check if port, baudrate and mode are valid values
 * 			  Returns a file descriptor that must be used for read/write operations
 */
int uart_open(char *port, int baudrate, char *mode){
	int baudr;
	int cbits = CS8;
	int cpar = 0;
	int ipar = IGNPAR;
	int bstop = 0;
	int fd_uart;


	//Check valid baudrate
	switch (baudrate){
		case      50 : baudr = B50;
	                   break;
	    case      75 : baudr = B75;
	                   break;
	    case     110 : baudr = B110;
	                   break;
	    case     134 : baudr = B134;
	                   break;
	    case     150 : baudr = B150;
	                   break;
	    case     200 : baudr = B200;
	                   break;
	    case     300 : baudr = B300;
	                   break;
	    case     600 : baudr = B600;
	                   break;
	    case    1200 : baudr = B1200;
	                   break;
	    case    1800 : baudr = B1800;
	                   break;
	    case    2400 : baudr = B2400;
	                   break;
	    case    4800 : baudr = B4800;
	                   break;
	    case    9600 : baudr = B9600;
	                   break;
	    case   19200 : baudr = B19200;
	                   break;
	    case   38400 : baudr = B38400;
	                   break;
	    case   57600 : baudr = B57600;
	                   break;
	    case  115200 : baudr = B115200;
	                   break;
	    case  230400 : baudr = B230400;
	                   break;
	    case  460800 : baudr = B460800;
	                   break;
	    case  500000 : baudr = B500000;
	                   break;
	    case  576000 : baudr = B576000;
	                   break;
	    case  921600 : baudr = B921600;
	                   break;
	    case 1000000 : baudr = B1000000;
	                   break;
	    case 1152000 : baudr = B1152000;
	                   break;
	    case 1500000 : baudr = B1500000;
	                   break;
	    case 2000000 : baudr = B2000000;
	                   break;
	    case 2500000 : baudr = B2500000;
	                   break;
	    case 3000000 : baudr = B3000000;
	                   break;
	    case 3500000 : baudr = B3500000;
	                   break;
	    case 4000000 : baudr = B4000000;
	                   break;
	    default      : printf("Invalid baudrate\n");
	    			   fflush(stdout);
	                   return(-1);
	                   break;
	}


	//Check valid mode
	if (strlen(mode) != 3){
		printf("Invalid mode \"%s\"\n", mode);
		fflush(stdout);
		return (-1);
	}

	switch(mode[0]){
	    case '8': cbits = CS8;
	              break;
	    case '7': cbits = CS7;
	              break;
	    case '6': cbits = CS6;
	              break;
	    case '5': cbits = CS5;
	              break;
	    default : printf("Invalid number of data-bits '%c'\n", mode[0]);
	    		  fflush(stdout);
	              return(-1);
	              break;
	}

	switch(mode[1]){
	    case 'N':
	    case 'n': cpar = 0;
	              ipar = IGNPAR;
	              break;
	    case 'E':
	    case 'e': cpar = PARENB;
	              ipar = INPCK;
	              break;
	    case 'O':
	    case 'o': cpar = (PARENB | PARODD);
	              ipar = INPCK;
	              break;
	    default : printf("Invalid parity '%c'\n", mode[1]);
	    		  fflush(stdout);
	              return(-1);
	              break;
	}

	switch(mode[2]){
	    case '1': bstop = 0;
	              break;
	    case '2': bstop = CSTOPB;
	              break;
	    default : printf("Invalid number of stop bits '%c'\n", mode[2]);
	    		  fflush(stdout);
	              return(-1);
	              break;
	}

	/* O_RDWR: Read & write */
	/* O_NOCTTY: Don't open a system terminal over this device */
	/* O_NDELAY: Non-block access. It should work the same with O_NONBLOCK */
	if ((fd_uart = open(port, O_RDWR | O_NOCTTY | /* O_NONBLOCK*/O_NDELAY)) < 0) {
		printf("Error opening device. Please, check device path is correct \n");
		fflush(stdout);
		return(-1);
	}

	//The flags (defined in /usr/include/termios.h):
	//	Baud rate:- B1200, B2400, B4800, B9600, B19200, B38400, B57600, B115200, B230400, B460800, B500000, B576000, B921600, B1000000, B1152000, B1500000, B2000000, B2500000, B3000000, B3500000, B4000000
	//	CSIZE:- CS5, CS6, CS7, CS8
	//	CLOCAL - Ignore modem status lines
	//	CREAD - Enable receiver
	//	IGNPAR = Ignore characters with parity errors
	//	ICRNL - Map CR to NL on input (Use for ASCII comms where you want to auto correct end of line characters - don't use for bianry comms!)
	//	PARENB - Parity enable
	//	PARODD - Odd parity (else even)

	struct termios opts; //Structure associated to parameters used in terminal configuration
	tcgetattr(fd_uart,&opts); //Get parameters associated to terminal
	bzero(&opts,sizeof(opts));//Set structure to all-zero

	opts.c_cflag = cbits | cpar | bstop | CLOCAL | CREAD;
	opts.c_iflag = ipar | ICRNL;
	opts.c_lflag = ICANON; 					  	  // Canonical input, desactivate echo and don't send signals to the program
	opts.c_oflag = 0;               			  // Raw output
	opts.c_cc[VMIN]  = 1;	        			  // Block reading until x characters are received
	opts.c_cc[VTIME] = 0;   	    			  // Block until a timer expires (n * 100 mSec.)

	cfsetispeed(&opts, baudr);
	cfsetospeed(&opts, baudr);

	tcflush(fd_uart,TCIFLUSH); //Clean modem line

	//Set new config
	if (tcsetattr(fd_uart,TCSANOW,&opts) < 0){
		printf("Error setting serial device configuration\n");
		fflush(stdout);
		close(fd_uart);
		return(-1);
	}

	return fd_uart;
} // UART OPEN END


/* UART_READ: Reads a certain number of bytes from a UART that must be previously opened.
 * 			  Returns a negative value in case of error. 0 if nothing to read. number of characters read in other case
 */
int uart_read(int fd, char *buf, int size){
	int n = 0;

	bzero(buf,size);
	n = read(fd, buf, size);

	#ifdef UART_DEBUG
	if (n>0){
		printf("Read %d bytes in fd %d\n",n,fd);
		int i=0;
		for (i=0;i<n;i++){
			printf("%c",buf[i]);
		}
		printf("\n");
		fflush(stdout);
	}
	#endif

	return n;
} // UART READ END


/* UART_WRITE_BYTE: Writes a byte in a serial port that must be previously opened.
 * 					Returns a negative value in case of error.
 */
int uart_write_byte(int fd, char byte){
	int n;
	n = write(fd, &byte, 1);
	if (n<0){
		printf("Error writing byte in UART\n");
		fflush(stdout);
		close(fd);
		return(-1);
	}
	return 0;
} // UART WRITE BYTE END


/* UART_WRITE_BUFFER: Writes a consecutive number of bytes in a serial port.
 * 					  Returns a negative value in case of error.
 */
int uart_write_buffer(int fd, char *buf, int size){
	int n;
	n = write(fd, buf, size);

	#ifdef UART_DEBUG
	printf("Writed %d bytes in fd %d\n",n,fd);
	fflush(stdout);
	#endif

	if (n<0){
		printf("Error writing buffer in UART\n");
		fflush(stdout);
		close(fd);
		return(-1);
	}
	return 0;
} // UART WRITE BUFFER END


/* UART_CLOSE: Close a uart which has been previously opened.
 * 			   Returns a negative value in case of error.
 */
int uart_close(int fd){
	close(fd);
	return 0;
} // UART CLOSE END
