#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <netinet/in.h>
#include <net/if.h>
#include "spi.h"


// =======================================================================
//
//  GLOBAL VARIABLES
//
// =======================================================================

int main(void) {

	int fd;
	struct ifreq ifr;
	struct sockaddr_in *addr;
	char *address;

	system("ifconfig > ifconfig.txt");

	//system("nmcli general status > status.txt");
	system("nmcli device status > status.txt");

	char ch, file_name[25];
	char *line_buf = NULL;
	size_t line_buf_size = 0;
	int line_count = 2;
	ssize_t line_size;
	FILE *fp;
	char *estados[5];
	int i = 0;
    char str[512];

	char *token;
	char *eth[4];
	char *wlan[4];
	int ethActive = 0;
	int wlanActive = 0;
	int gprsActive = 0;


	fp = fopen("status.txt", "r"); // read mode

	if (fp == NULL)
	{
	  perror("Error while opening the file.\n");
	  exit(EXIT_FAILURE);
	}

   while( fgets (str, sizeof(str), fp)!=NULL ) {
		if(strstr(str,"ethernet")) {
			fputs (str, stdout); /* write the line */
			token = strtok(str," "); 
		    while (token != NULL) { 
		    	eth[i] = token;
		       	//printf("Estado %s\n", eth[i]); 
		        token = strtok(NULL, " "); 
		        i++;
		    }
			if(strcmp(eth[2], "connected") == 0) {
				ethActive = 1;
				printf("activado eth\r\n");
				i = 0;
			}
			break;
		} 
		else if(strstr(str,"wifi")) {
			fputs (str, stdout); /* write the line */
			token = strtok(str," "); 
		    while (token != NULL) { 
		    	wlan[i] = token;
		       	//printf("Estado %s\n", wlan[i]); 
		        token = strtok(NULL, " "); 
		        i++;
		    }
			if(strcmp(wlan[2], "connected") == 0) {
				ethActive = 1;
				printf("activado eth\r\n");
				i = 0;
			}
			break;
		} 
		/*else {
			gprsActive = 1;
			printf("activado gprs\r\n");
			break;
		}*/

    //  puts(str); 
   }

   if(ethActive == 0 && wlanActive == 0){
   		gprsActive = 1;
   		printf("activado gprs\r\n");
   }

/*
	line_size = getline(&line_buf, &line_buf_size, fp);
	line_size = getline(&line_buf, &line_buf_size, fp);
	printf("line[%06d]: chars=%06zd, buf size=%06zu, contents: %s", line_count,
	    line_size, line_buf_size, line_buf);

	token = strtok(line_buf," "); 

    // Keep printing tokens while one of the 	
    // delimiters present in str[]. 
    while (token != NULL) { 
    	estados[i] = token;
       //printf("Estado %s\n", estados[i]); 
        token = strtok(NULL, " "); 
        i++;
    }

	fclose(fp);

	if (strcmp(estados[0], "connected") == 0) {
		//Hay ethernet
		fd = socket(AF_INET, SOCK_DGRAM, 0);

		ifr.ifr_addr.sa_family = AF_INET;

		snprintf(ifr.ifr_name, IFNAMSIZ, "eth0");

		ioctl(fd, SIOCGIFADDR, &ifr);

		addr = (struct sockaddr_in *)&(ifr.ifr_addr);
		address=inet_ntoa(addr->sin_addr);

		printf("%s\n", address);

		close(fd);
	} else if (strcmp(estados[3], "enabled") == 0){
		//El wifi esta enabled

	}*/

}

