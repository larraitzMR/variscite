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
	FILE *fp2;
	int i = 0;
    char str[512];


	char *token;
	char *estado;
	char *eth[4];
	char *wlan[4];
	int ethActive = 0;
	int wlanActive = 0;
	int gprsActive = 0;
	char ipAdd[18];
	char *ipFinal;


	fp = fopen("status.txt", "r"); // read mode

	if (fp == NULL)
	{
	  perror("Error while opening the file.\n");
	  exit(EXIT_FAILURE);
	}

   while( fgets (str, sizeof(str), fp)!=NULL ) {
		if(strstr(str,"ethernet")) {
			printf("ethernet\n"); 
			fputs (str, stdout); /* write the line */
			token = strtok(str," "); 
			i = 0;
		    while (token != NULL) { 
		    	eth[i] = token;
		       	//printf("Estado %s\n", eth[i]); 
		        token = strtok(NULL, " "); 
		        i++;
		    }
		    if(strcmp(eth[2], "unavailable") == 0) {
				printf("ethernet disconnected\r\n");
			}
			else if(strcmp(eth[2], "connected") == 0) {
				ethActive = 1;
				printf("activado eth\r\n");
				break;
			}

		} 
		if(strstr(str,"wifi")) {
			printf("wifi\n"); 
			fputs (str, stdout); /* write the line */
			token = strtok(str," "); 
			i = 0;
		    while (token != NULL) { 
		    	wlan[i] = token;
		       	//printf("Estado %s\n", wlan[i]); 
		        token = strtok(NULL, " "); 
		        i++;
		    }
		   	estado = *&wlan[2];	
		    if (strcmp(estado, "unavailable") == 0) {
				printf("unavailable\n"); 
				system("nmcli radio wifi on");
				system("nmcli device status > status.txt");

				system("nmcli device wifi connect MYRUNS password RunMyRuns841");
			   	if (strstr(system("nmcli connection show --active"), "wlan0")){
			   		printf("encontrado wifi\r\n");
			   	}
			}
	   		if (strcmp(estado, "disconnected") == 0){
			   	printf("wifi disconnected\n");

			   	//conectarse al wifi
			   	system("nmcli device wifi connect MYRUNS password RunMyRuns841");
			   	if (strstr(system("nmcli connection show --active"), "wlan0")){
			   		printf("encontrado wifi\r\n");
			   	}
			}
			if(strcmp(estado, "connected") == 0) {
				wlanActive = 1;
				printf("activado wifi\r\n");
				break; 
			}
			
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

	fclose(fp); */

  	system("nmcli > connecDevices.txt");

	fp2 = fopen("connecDevices.txt", "r"); // read mode

	if (fp2 == NULL)
	{
	  perror("Error while opening the file.\n");
	  exit(EXIT_FAILURE);
	}
 	while( fgets (str, sizeof(str), fp2)!=NULL ) {
		fputs (str, stdout); /* write the line */
 		if(strstr(str,"inet4")) {
 			 strncpy (ipAdd, str + 6, 16);
 			 	//printf("IP: %s\n", ipAdd);

 			 break;
 		}
 	}

	ipFinal = strtok(ipAdd, "/");
	printf("IP: %s\n", ipFinal);

	


	if (ethActive == 1) {
		//Hay ethernet
	/*	fd = socket(AF_INET, SOCK_DGRAM, 0);

		ifr.ifr_addr.sa_family = AF_INET;

		snprintf(ifr.ifr_name, IFNAMSIZ, "eth0");

		ioctl(fd, SIOCGIFADDR, &ifr);

		addr = (struct sockaddr_in *)&(ifr.ifr_addr);
		address=inet_ntoa(addr->sin_addr);

		printf("%s\n", address);

		close(fd); */
		 	printf("IP: %s\n", ipFinal);
	} else if (wlanActive == 1){
		//El wifi esta enabled
		 	printf("IP: %s\n", ipFinal);

	}

}
