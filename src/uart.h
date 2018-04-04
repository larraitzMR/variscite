/*
 * uart.h
 *
 *  Created on: Oct 4, 2017
 *      Author:xabi1.1
 */

#ifndef UART_H_
#define UART_H_

#include <stdio.h>
#include <string.h>
#include <termios.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <fcntl.h>

// =======================================================================
//
//  DEFINES
//
// =======================================================================
//#define UART_DEBUG


// =======================================================================
//
//  PROTOTYPES
//
// =======================================================================
int uart_open(char *port, int baudrate, char *mode); //Returns file descriptor
int uart_read(int fd, char *buf, int size);
int uart_write_byte(int fd, char byte);
int uart_write_buffer(int fd, char *buf, int size);
int uart_close(int fd);

#endif /* UART_H_ */
