/*
 * network.h
 *
 *  Created on: Nov 2, 2017
 *      Author: xabi1.1 */

#ifndef NETWORK_H_
#define NETWORK_H_

// =======================================================================
//
//  DEFINES
//
// =======================================================================
#define NETWORK_DEBUG

// =======================================================================
//
//  PROTOTYPES
//
// =======================================================================
int configure_udp_socket(int port);
void enviar_udp_msg(int socket_fd, char *data, int port);
int send_udp_msg(int sock_descriptor, char *ip, int port, char *data, short len);
int send_udp_msg_checksum(int sock_descriptor, char *ip, int port, char *data, short len);
int read_udp_message(int sock_descriptor, char *message, char len);
int connect_tcp_remote(char *ip, int port);
void close_tcp_socket(int fd);
int read_tcp_message(int sock_descriptor, char *message, char len);
int send_tcp_msg(int sock_descriptor, char *data, short len);
char network_checksum(char *data, short len);

#endif /* NETWORK_H_ */
