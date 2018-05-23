/*
 * network.c
 *
 *  Created on: Nov 2, 2017
 *      Author: xabi1.1
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/socket.h>		// UDP and TCP socket
#include <netinet/in.h>		// in_addr struct
#include <fcntl.h>			// To configure socket
#include <arpa/inet.h>      // inet_ntoa() for IP format
#include <errno.h>			// For error handling
#include "network.h"





// =======================================================================
//
//  GLOBAL VARIABLES
//
// =======================================================================
struct sockaddr_in local_socket; //Configuration for local socket




// =======================================================================
//
//  FUNCTIONS
//
// =======================================================================

/* Configure a local UDP listen socket in a given port.
 * Return socket descriptor or -1 in case of error
 */
int configure_udp_socket(int port){
	int flags;
	int descriptor;

	//Create UDP SOCKET
	if((descriptor = socket(AF_INET,SOCK_DGRAM,0)) == -1){
		return -1;
	}

	//Socket configuration:
	//	- Accept connections from any host
	//	- Listen on the given port
	bzero(&local_socket,sizeof(local_socket));
	local_socket.sin_family = AF_INET;
	local_socket.sin_addr.s_addr = htonl(INADDR_ANY);
	local_socket.sin_port = htons(port);
	if (bind(descriptor, (struct sockaddr *)&local_socket, sizeof(local_socket)) == -1){
		shutdown(descriptor,SHUT_RDWR);
		return -1;
	}

	//Make socket NON-BLOCK: recvfrom() returns with EWOULDBLOCK
	flags = fcntl(descriptor,F_GETFL);
	flags |= O_NONBLOCK;
	fcntl(descriptor,F_SETFL,flags);

	return descriptor;
} // CONFIGURE_UDP_SOCKET END

void enviar_udp_msg(int socket_fd, char *data, int port) {
	int errors = 0;
	if (send_udp_msg(socket_fd, IP_ADDRESS, port, data, strlen(data)) < 0) {
#ifdef RFID_DEBUG
		printf("geo_rfid: ERROR SENDING UDP MESSAGE\n");
		fflush(stdout);
#endif
		errors++;
	}
}

/* Send UDP message to remote receiver */
int send_udp_msg(int sock_descriptor, char *ip, int port, char *data, short len){
	struct sockaddr_in remote_socket; //Keeps connection information on remote side
	char message[64];
	short i;

	bzero(&remote_socket,sizeof(remote_socket));

	//Configure message receiver address
	remote_socket.sin_family = AF_INET;
	remote_socket.sin_port = htons(port);
	if (inet_aton(ip, &remote_socket.sin_addr) == 0){
	    return -1;
	}

	// MESSAGE FORMAT:
	// * | MSG_LEN(1b) | CO(1b) | DATA(Nb) | CHECKSUM(1b)

	//Empty message
	bzero(message,sizeof(message));

	//Data
	for (i=0;i<len;i++){
		message[i] = data[i];
	}

	//Checksum
	//message[len] = network_checksum(message,len);


	//DEBUG
	#ifdef NETWORK_DEBUG
	for (i=0; i<len+1; i++){
		printf("%02x:",(unsigned char)message[i]);
	}
	printf("\n");
	fflush(stdout);
	#endif

	//Send message
	if (sendto(sock_descriptor,message, len+1, 0, (struct sockaddr *)&remote_socket, sizeof(remote_socket)) < 0){
		return -1;
	}

	return 0;
} // SEND_UDP_MSG END

/* Send UDP message to remote receiver */
int send_udp_msg_checksum(int sock_descriptor, char *ip, int port, char *data, short len){
	struct sockaddr_in remote_socket; //Keeps connection information on remote side
	char message[64];
	short i;

	bzero(&remote_socket,sizeof(remote_socket));

	//Configure message receiver address
	remote_socket.sin_family = AF_INET;
	remote_socket.sin_port = htons(port);
	if (inet_aton(ip, &remote_socket.sin_addr) == 0){
	    return -1;
	}

	// MESSAGE FORMAT:
	// * | MSG_LEN(1b) | CO(1b) | DATA(Nb) | CHECKSUM(1b)

	//Empty message
	bzero(message,sizeof(message));

	//Data
	for (i=0;i<len;i++){
		message[i] = data[i];
	}

	//Checksum
	message[len] = network_checksum(message,len);


	//DEBUG
	#ifdef NETWORK_DEBUG
	for (i=0; i<len+1; i++){
		printf("%02x:",(unsigned char)message[i]);
	}
	printf("\n");
	fflush(stdout);
	#endif

	//Send message
	if (sendto(sock_descriptor,message, len+1, 0, (struct sockaddr *)&remote_socket, sizeof(remote_socket)) < 0){
		return -1;
	}

	return 0;
} // SEND_UDP_MSG END

/* Read (and parse) messages from UDP socket */
int read_udp_message(int sock_descriptor, char *message, char len){
	struct sockaddr_in remote_socket; //Keeps connection information on client side
	unsigned int remote_server_len = sizeof(remote_socket);

	int rc; //Number of chars received
	//int len=0;

	bzero(message,len);
	rc = recvfrom(sock_descriptor, message, 64, 0, (struct sockaddr *)&remote_socket,&remote_server_len);

	//Check if there was a real error, if EWOULDBLOCK is retourned, is because socket is NON-BLOCK
	if (rc < 0){
		if (errno!=EWOULDBLOCK){
			#ifdef NETWORK_DEBUG
			printf("network: REAL ERROR WHEN READING AN UDP SOCKET\n");
			fflush(stdout);
			#endif
			return -1;
		}
	}

	//Process packet
	if (rc>0){
		// Test checksum
		if (message[rc-1] != network_checksum(message,rc-1)){
			#ifdef NETWORK_DEBUG
			printf("network: WRONG CHECKSUM IN RECEIVED UDP MESSAGE\n");
			fflush(stdout);
			#endif
			return -1;
		}

		// Test fist character
		if (message[0] != '*'){
			#ifdef NETWORK_DEBUG
			printf("network: WRONG FIRST CHARACTER IN UDP MESSAGE\n");
			fflush(stdout);
			#endif
			return -1;
		}

	}

	return rc;
} // READ_MESSAGE END


/* Connect to a TCP socket in a given ip & port.
 * Return socket descriptor or -1 in case of error
 */
int connect_tcp_remote(char *ip, int port){
	int sockfd;
	struct sockaddr_in remote_socket; //Keeps connection information on client side

	 if ((sockfd=socket(AF_INET,SOCK_STREAM,0)) < 0){
		 return (-1);
	 }

	 bzero(&remote_socket,sizeof(remote_socket));

	 remote_socket.sin_family=AF_INET;
	 remote_socket.sin_port=htons(port);
	 if (inet_pton(AF_INET,ip,&(remote_socket.sin_addr)) == 0){
		 return(-1);
	 }

	 if (connect(sockfd,(struct sockaddr *)&remote_socket,sizeof(remote_socket)) < 0){
		 return (-1);
	 }

	 return sockfd;
} // CONNECT_TCP_REMOTE END



/* Close a tcp connection */
void close_tcp_socket(int fd){
	close(fd);
} // CLOSE_TCP_SOCKET END


/* Read (and parse) messages from TCP socket */
int read_tcp_message(int sock_descriptor, char *message, char len){

	int rc; //Number of chars received
	//int len=0;

	bzero(message,len);
	rc=read(sock_descriptor,message,len);

	//Check if there was a real error, if EWOULDBLOCK is retourned, is because socket is NON-BLOCK
	if (rc < 0){
		if (errno!=EWOULDBLOCK){
			#ifdef NETWORK_DEBUG
			printf("network: REAL ERROR WHEN READING TCP SOCKET\n");
			fflush(stdout);
			#endif
			return -1;
		}
	}

	//Process packet
	if (rc>0){
		// Test checksum
		if (message[rc-1] != network_checksum(message,rc-1)){
			#ifdef NETWORK_DEBUG
			printf("network: WRONG CHECKSUM IN RECEIVED TCP MESSAGE\n");
			fflush(stdout);
			#endif
			return -1;
		}

		// Test fist character
		if (message[0] != '*'){
			#ifdef NETWORK_DEBUG
			printf("network: WRONG FIRST CHARACTER IN TCP MESSAGE\n");
			fflush(stdout);
			#endif
			return -1;
		}

	}

	return rc;
} // READ_MESSAGE END


/* Send TCP message to remote receiver */
int send_tcp_msg(int sock_descriptor, char *data, short len){
	short i;
	char message[512];

	// MESSAGE FORMAT:
	// * | MSG_LEN(1b) | CO(1b) | DATA(Nb) | CHECKSUM(1b)

	//Empty message
	bzero(message,sizeof(message));

	//Data
	for (i=0;i<len;i++){
		message[i] = data[i];
	}

	//Checksum
	message[len] = network_checksum(message,len);


	//DEBUG
	#ifdef NETWORK_DEBUG
	for (i=0; i<len+1; i++){
		printf("%02x:",(unsigned char)message[i]);
	}
	printf("\n");
	fflush(stdout);
	#endif

	//Send message
	if (write(sock_descriptor,message, len+1) < 0){
		return -1;
	}

	return 0;
} // SEND_TCP_MSG END



char network_checksum(char *data, short len){
	char result = 0;
	short i = 0;

	for (i=0;i<len;i++){
		result += data[i];
	}

	//Two - complement
	result = ~result;
	result++;

	return result;
} // NETWORK_CHECKSUM END
