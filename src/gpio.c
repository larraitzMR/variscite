/*
 * gpio.c
 *
 *  Created on: Oct 4, 2017
 *      Author: xabi1.1
 */


#include "gpio.h"

// =======================================================================
//
//  LIBRARY FUNCTIONS
//
// =======================================================================

/* GPIO_CONFIG: Export and configure gpio as input or output */
int gpio_config(int port, int direction){
	FILE *file;
	char path[100];

	//Export port
	file = fopen("/sys/class/gpio/export","w");
	if (file == NULL){
		printf("Error setting gpio %d\n",port);
		fflush(stdout);
		return(-1);
	}
	fprintf(file,"%d",port);
	fclose(file);

	//Configure as input or output
	sprintf(path,"/sys/class/gpio/gpio%d/direction",port);
	file = fopen (path,"w");
	if (file == NULL){
		printf("Error setting gpio %d\n",port);
		fflush(stdout);
		return(-1);
	}

	if (direction == GPIO_INPUT){
		fprintf(file,"in");
	}else{
		fprintf(file,"out");
	}

	fclose(file);
	return 0;
} // GPIO CONFIG END


/* GPIO_READ: Read current value from a gpio configured as input
 * 			  Returns 0, 1 or negative value if error.
 */
int gpio_read(int port){
	FILE *file;
	char path[100];
	char line[3];
	int value;

	sprintf(path,"/sys/class/gpio/gpio%d/value",port);
	file = fopen (path,"r");
	if (file == NULL){
		printf("Error reading gpio %d\n",port);
		fflush(stdout);
		return(-1);
	}
	fgets(line,3,file);
	fclose(file);

	value = atoi(line);

	return value;
} // GPIO READ END


/* GPIO_WRITE: Change current state in a gpio output
 * 			   Returns written value or negative if error.
 */
int gpio_write(int port, int value){
	FILE *file;
	char path[100];

	switch (value){
		case 0:
		case 1:
			break;
		default:
			printf("Invalid gpio value %d\n",value);
			fflush(stdout);
			return(-1);
	}

	sprintf(path,"/sys/class/gpio/gpio%d/value",port);
	file = fopen (path,"w");
	if (file == NULL){
		printf("Error writing gpio %d\n",port);
		fflush(stdout);
		return(-1);
	}
	fprintf(file,"%d",value);
	fclose(file);

	return value;
} // GPIO WRITE END
