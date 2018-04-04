/*
 * Copyright (c) 2017 Fixso.
 * All rights reserved.
 *
 * This is the HAL to run LMIC on top of MX6-Dart Variscite SoM
 * NOTE: The lmic library must be compiled with -std=gnu99 FLAG to work
 */

//#define __USE_POSIX199309 1
#include <unistd.h>
#include <string.h>
#include <time.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <linux/spi/spidev.h>
#include "config.h"
#include "oslmic.h"
#include "hal_lmic.h"
#include "../gpio.h"

// -----------------------------------------------------------------------------
// I/O

#define NUM_DIO			3

#define LMIC_UNUSED_PIN	255

//SPI SETTINGS
#define LMIC_SPI					"/dev/spidev2.0"
#define LMIC_SPI_SPEED				5000000									//5MHz
#define LMIC_SPI_BITS_PER_WORD		8
#define LMIC_SPI_MODE				SPI_MODE_0

//PINMAP
#define LMIC_NSS		LMIC_UNUSED_PIN /*120*/ 				//This pin seems to be controlled by the spi driver
#define LMIC_RST		85
#define LMIC_RXTX		75
#define LMIC_DIO0		86
#define LMIC_DIO1		65
#define LMIC_DIO2		LMIC_UNUSED_PIN


// -----------------------------------------------------------------------------
// I/O

static void hal_io_init () {
    // TODO: clock enable for GPIO ports A,B,C


    // configure output lines and set to low state
	if (LMIC_NSS != LMIC_UNUSED_PIN){
		gpio_config(LMIC_NSS, GPIO_OUTPUT);
	}

	if (LMIC_RST != LMIC_UNUSED_PIN){
		gpio_config(LMIC_RST, GPIO_OUTPUT);
	}

	if (LMIC_RXTX != LMIC_UNUSED_PIN){
		gpio_config(LMIC_RXTX, GPIO_OUTPUT);
	}

    // configure input lines and register IRQ
    if (LMIC_DIO0 != LMIC_UNUSED_PIN){
		gpio_config(LMIC_DIO0, GPIO_INPUT);
	}

    if (LMIC_DIO1 != LMIC_UNUSED_PIN){
    	gpio_config(LMIC_DIO1, GPIO_INPUT);
    }

    if (LMIC_DIO2 != LMIC_UNUSED_PIN){
   		gpio_config(LMIC_DIO2, GPIO_INPUT);
   	}
}

// val ==1  => tx 1, rx 0 ; val == 0 => tx 0, rx 1
void hal_pin_rxtx (u1_t val) {
	if (LMIC_RXTX != LMIC_UNUSED_PIN)
		gpio_write(LMIC_RXTX, val);
}



// set radio RST pin to given value (or keep floating!)
void hal_pin_rst (u1_t val) {
	if (LMIC_RST == LMIC_UNUSED_PIN)
		return;

	if ((val == 0) || (val == 1)){
		//Drive pin
		gpio_config(LMIC_RST, GPIO_OUTPUT);
		gpio_write(LMIC_RST, val);
	}else{
		//Keep pin floating
		gpio_config(LMIC_RST, GPIO_INPUT);
	}
}

// instead of using interrupts , perform polling of DIO PINS

static unsigned short dio_states[NUM_DIO] = {0};

static void hal_io_check() {
	int value;

	if (LMIC_DIO0 != LMIC_UNUSED_PIN){
		value = gpio_read(LMIC_DIO0);
		if ((value != -1) && (dio_states[0] != value)){
			dio_states[0] = value;
			if (dio_states[0])
				radio_irq_handler(0);
		}
	}

	if (LMIC_DIO1 != LMIC_UNUSED_PIN){
		value = gpio_read(LMIC_DIO1);
		if ((value != -1) && (dio_states[1] != value)){
			dio_states[1] = value;
			if (dio_states[1])
				radio_irq_handler(1);
		}
	}

	if (LMIC_DIO2 != LMIC_UNUSED_PIN){
		value = gpio_read(LMIC_DIO2);
		if ((value != -1) && (dio_states[2] != value)){
			dio_states[2] = value;
			if (dio_states[2])
				radio_irq_handler(2);
		}
	}

}

// -----------------------------------------------------------------------------
// SPI

static int spi_descriptor;

// open and configure spi port
static void hal_spi_init () {
	unsigned char spi_mode = LMIC_SPI_MODE;
	unsigned char spi_bits = LMIC_SPI_BITS_PER_WORD;
	unsigned int spi_speed = LMIC_SPI_SPEED;
	int ret;

	spi_descriptor = open(LMIC_SPI, O_RDWR);
	if (spi_descriptor < 0) return;

	/*
	 * spi mode
	 */
	ret = ioctl(spi_descriptor, SPI_IOC_WR_MODE, &spi_mode);
	if (ret == -1) return;

	ret = ioctl(spi_descriptor, SPI_IOC_RD_MODE, &spi_mode);
	if (ret == -1) return;

	/*
	 * bits per word
	 */
	ret = ioctl(spi_descriptor, SPI_IOC_WR_BITS_PER_WORD, &spi_bits);
	if (ret == -1) return;

	ret = ioctl(spi_descriptor, SPI_IOC_RD_BITS_PER_WORD, &spi_bits);
	if (ret == -1) return;

	/*
	 * max speed hz
	 */
	ret = ioctl(spi_descriptor, SPI_IOC_WR_MAX_SPEED_HZ, &spi_speed);
	if (ret == -1) return;

	ret = ioctl(spi_descriptor, SPI_IOC_RD_MAX_SPEED_HZ, &spi_speed);
	if (ret == -1) return;

	return;
}


// set radio NSS pin to given value
void hal_pin_nss (u1_t val) {
	if (LMIC_NSS != LMIC_UNUSED_PIN)
			gpio_write(LMIC_NSS, val);
}


// perform SPI transaction with radio
u1_t hal_spi (u1_t out) {
	u1_t ret=0;
	struct spi_ioc_transfer xfer[1];
	int status;
	memset(&xfer, 0, sizeof xfer);

	xfer[0].tx_buf = (unsigned long)out;
	xfer[0].rx_buf = (unsigned long)ret;
	xfer[0].len = 1; //Only read/write 1 byte
	xfer[0].delay_usecs = 0;
	status = ioctl(spi_descriptor, SPI_IOC_MESSAGE(1), &xfer);
	if (status < 0) {
		return 0;
	}

	return ret;
}



// -----------------------------------------------------------------------------
// TIME

struct timespec tstart={0,0};

static void hal_time_init () {
	clock_gettime(CLOCK_MONOTONIC_RAW, &tstart);
	tstart.tv_nsec=0; //Makes difference calculations in hal_ticks() easier
}

u4_t hal_ticks () {
	// LMIC requires ticks to be 15.5μs - 100 μs long
	struct timespec ts;
	clock_gettime(CLOCK_MONOTONIC_RAW, &ts);
	ts.tv_sec-=tstart.tv_sec;
	u8_t ticks=ts.tv_sec*(1000000/US_PER_OSTICK)+ts.tv_nsec/(1000*US_PER_OSTICK);
	return (u4_t)ticks;
}

// return modified delta ticks from now to specified ticktime (0 for past)
static u2_t deltaticks (u4_t time) {
    u4_t t = hal_ticks();
    s4_t d = time - t;
    if( d<=5 ){      // in order to compensate multi-tasking and other delays, taken from raspi library
    	return 0;    // in the past
    }else{
    	return (u2_t)d;
    }
}


// wait until specified timestamp (in ticks) is reached.
void hal_waitUntil (u4_t time) {
	u4_t delta = deltaticks(time);

	if (delta==0){
		return;
	}else{
		while (delta>MAX_USLEEP_TICKS){
			usleep(1000000);
			delta -= MAX_USLEEP_TICKS;
		}

	    usleep(delta*US_PER_OSTICK);
	}

	return;
}

// check and rewind for target time
u1_t hal_checkTimer (u4_t time) {
	// No need to schedule wakeup, since we're not sleeping
	    return deltaticks(time) <= 0;
}


// -----------------------------------------------------------------------------
// IRQ

static u8_t irqlevel = 0;

void hal_disableIRQs () {
    irqlevel++;
}

void hal_enableIRQs () {
    if(--irqlevel == 0) {
    	// Instead of using proper interrupts (which are a bit tricky
    	// and/or not available on all pins on AVR), just poll the pin
    	// values. Since os_runloop disables and re-enables interrupts,
    	// putting this here makes sure we check at least once every
    	// loop.
    	//
    	// As an additional bonus, this prevents problems resulting
    	// from running SPI transfers inside ISRs
    	hal_io_check();
    }
}

void hal_sleep () {
    //Not implemented
}

// -----------------------------------------------------------------------------

void hal_init () {

    // configure radio I/O and interrupt handler
    hal_io_init();
    // configure radio SPI
    hal_spi_init();
    // configure timer and interrupt handler
    hal_time_init();


}

void hal_failed () {
	printf("FATAL ERROR IN LMIC HAL! Halting system...\n");
    // HALT...
    hal_disableIRQs();
    while(1);
}


