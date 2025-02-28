#ifndef UDP_FILE_TRANSFER_H
#define UDP_FILE_TRANSFER_H

// Define the maximum sizes for the filename, data, and error message.
#define MAX_FILENAME_SIZE 256      
#define MAX_DATA_SIZE     512      
#define MAX_MODE_SIZE     10      
#define MAX_ERROR_MSG_SIZE 100    

#include <stdint.h>

// Define the TFTP operation codes.
typedef enum {
    RRQ   = 1,  // Read Request
    WRQ   = 2,  // Write Request
    DATA  = 3,  // Data Packet 
    ACK   = 4,  // Acknowledgment
    ERROR = 5,  // Error Packet
    DEL   = 6   // Delete Request
} TftpOpCode;

// Represent the TFTP request packet.
typedef struct {
    uint16_t opcode;                        
    char filename[MAX_FILENAME_SIZE];       
    char mode[MAX_MODE_SIZE];               
} TftpRequestPacket;

// Represent the TFTP data packet.
typedef struct {
    uint16_t opcode;       
    uint16_t blockNumber;  
    uint8_t data[MAX_DATA_SIZE]; 
} TftpDataPacket;

// Represent the TFTP acknowledgment packet.
typedef struct {
    uint16_t opcode;       
    uint16_t blockNumber;  
} TftpAckPacket;

// Represent the TFTP error packet.
typedef struct {
    uint16_t opcode;        
    uint16_t errorCode;                 // Error code for future use.
    char errorMsg[MAX_ERROR_MSG_SIZE]; 
} TftpErrorPacket;

// Represent the TFTP delete packet.
typedef struct {
    uint16_t opcode;        
    char filename[MAX_FILENAME_SIZE]; 
} TftpDeletePacket;

#endif // UDP_FILE_TRANSFER_H



