#ifndef UDP_CLIENT_H
#define UDP_CLIENT_H

#include <netinet/in.h>        // For struct sockaddr_in and related types
#include "udp_file_transfer.h" // For Tftp* structures and constants

// Client constants
#define SERVER_IP "127.0.0.1"
#define SERVER_PORT 8080
#define BUFFER_SIZE 516
#define MAX_RETRIES 5

// Function prototypes
int create_sockfd(void);
void set_socket_timeout(int sockfd);
void print_help_menu(void);
void print_prompt(void);
char * set_transfer_mode(void);
void set_ip(struct sockaddr_in *server_addr);
void set_port(struct sockaddr_in *server_addr);
void del_file(struct sockaddr_in *server_addr);
void get_file(char *mode, struct sockaddr_in *server_addr);
void set_file(char *mode, struct sockaddr_in *server_addr);

#endif // UDP_CLIENT_H
