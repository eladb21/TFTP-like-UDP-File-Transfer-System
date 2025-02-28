#ifndef UDP_SERVER_H
#define UDP_SERVER_H

#include <netinet/in.h>  // for struct sockaddr_in
#include "udp_file_transfer.h"  // for Tftp* definitions

// Constants
#define SERVER_PORT 8080
#define BUFFER_SIZE 516
#define MAX_RETRIES 5

// Function prototypes
void set_socket_timeout(int sockfd);
int create_udp_socket(int port);
void send_file(int sockfd, TftpRequestPacket *req, struct sockaddr_in *client_addr, socklen_t addr_len, char *base_dir);
void set_file(int sockfd, TftpRequestPacket *req, struct sockaddr_in *client_addr, socklen_t addr_len, char *base_dir);
void delete_file(int sockfd, TftpDeletePacket *delPkt, struct sockaddr_in *client_addr, socklen_t addr_len, char *base_dir);
void listen_for_requests(int sockfd, char *base_dir);

#endif // UDP_SERVER_H
