/*
 * gpio.h
 *
 *  Created on: Oct 4, 2017
 *      Author: xabi1.1
 */

#ifndef GPIO_H_
#define GPIO_H_

#include <stdio.h>
#include <stdlib.h>


// =======================================================================
//
//  DEFINES
//
// =======================================================================
#define GPIO_INPUT 		0x01
#define GPIO_OUTPUT		0x02


// =======================================================================
//
//  PROTOTYPES
//
// =======================================================================
int gpio_config(int port, int direction);
int gpio_read(int port);
int gpio_write(int port, int value);


#endif /* GPIO_H_ */
