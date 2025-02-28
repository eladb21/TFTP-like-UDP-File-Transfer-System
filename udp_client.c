#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/time.h>
#include <errno.h>
#include "Includes/udp_client.h"

int create_sockfd()
{
    // Create a UDP socket
    int sockfd;
    if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0)
    {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }
    return sockfd;
}

void set_socket_timeout(int sockfd)
{
    struct timeval timeout;
    timeout.tv_sec = 3;
    timeout.tv_usec = 0;
    if (setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)) < 0)
    {
        perror("Set timeout failed");
        close(sockfd);
        exit(EXIT_FAILURE);
    }
}

void print_help_menu()
{
	// print help message to the stdout
	fprintf(stdout, "\n> Options:\n"
        "   \n\t\tSETTINGS\n\n"
		"   mode <mode>\t\tSets the transfer mode to be used: "
		"<txt> for text mode and <bin> for binary mode, bin is default.\n"
        "   ip <ip>\t\tSets the server ip, default is 127.0.0.1\n"
        "   port <port>\t\tSets the server port, default is 8080\n"
        "   \n\t\tCOMMANDS\n\n"
        "   del <filename>\tDeletes the file identified by <filename> \n"
		"   get <src> <dest>\tTransfers the file identified by "
		"<src> from the server and saves it as <dst>.\n"
        "   set <src> <dest>\tTransfers the file identified by "
		"<src> to the server and saves it as <dst>.\n"
        "   \n\t\tOTHER\n\n"   
        "   help\t\t\tPrint this help menu.\n"
		"   quit\t\t\tQuit and close the client.\n");
}

void print_prompt()
{
	fprintf(stdout, "\n> ");
}

char * set_transfer_mode()
{
    // handle mode command
    char mode[516];

	// retrieve transfer mode from stdin
	scanf("%515s", mode);

    if (strncmp(mode, "txt", 3) == 0)
    {
        // set transfer mode to text
        fprintf(stdout, "Transfer mode set to text.\n");
        return "netascii"; 
    }
    else if (strncmp(mode, "bin", 3) == 0)
    {
        // set transfer mode to binary
        fprintf(stdout, "Transfer mode set to binary.\n");
        return "octet";
    }
    else
    {
        // print error message
        fprintf(stderr, "Invalid transfer mode: %s\n Transfer mode set to binary\n", mode);
        return "octet";
    }
}

void set_ip(struct sockaddr_in *server_addr)
{
    // handle ip command
    char ip[516];

    // retrieve ip address from stdin
    scanf("%515s", ip);

    // set ip address
    fprintf(stdout, "IP address set to %s.\n", ip);
    server_addr->sin_addr.s_addr = inet_addr(ip);
}

void set_port(struct sockaddr_in *server_addr)
{
    // handle port command
    int port;

    // retrieve port number from stdin
    scanf("%d", &port);

    // set port number
    fprintf(stdout, "Port number set to %d.\n", port);
    server_addr->sin_port = htons(port);
}

void del_file(struct sockaddr_in *server_addr)
{
    // handle del command
    char filename[MAX_FILENAME_SIZE];
    char buffer[BUFFER_SIZE];

    // Create a UDP socket
    int sockfd = create_sockfd();

    set_socket_timeout(sockfd);

    // retrieve filename from stdin
    scanf("%255s", filename);

    TftpDeletePacket deletePacket;

    // set opCode to DEL
    deletePacket.opcode = htons(DEL);

    // set filename and mode
    strncpy(deletePacket.filename, filename, MAX_FILENAME_SIZE);

    // Send message to server
    sendto(sockfd, &deletePacket, sizeof(deletePacket), 0, (const struct sockaddr *)server_addr, sizeof(*server_addr));

    // Receive response from server
    printf("Deleting file %s...\n", filename);
    int n = recvfrom(sockfd, buffer, BUFFER_SIZE, 0, NULL, NULL);
    if (n < 0)
    {
        perror("Receive failed");
    }
    else
    {
        buffer[n] = '\0';
        printf("Server: %s\n", buffer);
    }

    close(sockfd);
}

void get_file(char * mode, struct sockaddr_in *server_addr)
{
    // handle get command
    char src[MAX_FILENAME_SIZE];
    char dest[MAX_FILENAME_SIZE];

    // retrieve source and destination filenames from stdin
    scanf("%255s %255s", src, dest);
    fprintf(stdout, "src: %s, dest: %s\n", src, dest);

    // Create a UDP socket
    int sockfd = create_sockfd();
    set_socket_timeout(sockfd);

    TftpRequestPacket requestPacket;
    requestPacket.opcode = htons(RRQ);

    // set filename and mode
    strncpy(requestPacket.filename, src, MAX_FILENAME_SIZE);
    requestPacket.filename[MAX_FILENAME_SIZE - 1] = '\0';
    strncpy(requestPacket.mode, mode, MAX_MODE_SIZE);
    requestPacket.mode[MAX_MODE_SIZE - 1] = '\0';

    // Create a file
    FILE *file;
    if (strncmp(mode, "netascii", 8) == 0)
    {
        file = fopen(dest, "wt");
    }
    else
    {
        file = fopen(dest, "wb");
    }
    if (!file)
    {
        perror("File creation failed");
        close(sockfd);
        exit(EXIT_FAILURE);
    }
    
    if (sendto(sockfd, &requestPacket, sizeof(requestPacket), 0, (const struct sockaddr *)server_addr, sizeof(*server_addr)) < 0)
    {
        perror("Request sending failed");
        close(sockfd);
        fclose(file);
        exit(EXIT_FAILURE);
    }

    printf("Downloading file %s...\n", dest);
    TftpDataPacket dataPkt;
    TftpAckPacket ackPkt;
    uint16_t expectedBlock = 1;
    uint16_t lastAckBlock = 0;
    int unexpectedBlocks = 0;

    while (1)
    {

        int retries = 0;
        int n;

        // Receive data packet
        while (retries < MAX_RETRIES)
        {
            n = recvfrom(sockfd, &dataPkt, sizeof(dataPkt), 0, NULL, NULL);
            if (n < 0)
            {
                if (errno == EAGAIN || errno == EWOULDBLOCK)
                {
                    fprintf(stderr, "Timeout, retrying again...\n");
                    ackPkt.opcode = htons(ACK);
                    ackPkt.blockNumber = htons(lastAckBlock);
                    sendto(sockfd, &ackPkt, sizeof(ackPkt), 0, (const struct sockaddr *)server_addr, sizeof(*server_addr));
                    retries++;
                    continue;
                }
                else
                {
                    perror("Receive failed");
                    goto cleanup;
                }
            }
            else
            {
                break;
            }

        }
        if (retries == MAX_RETRIES)
        {
            fprintf(stderr, "Max retries reached\n");
            break;
        }

        uint16_t opCode = ntohs(dataPkt.opcode);
        uint16_t recBlockNumber = ntohs(dataPkt.blockNumber);

        fprintf(stdout, "Received %d bytes\n", n);
        fprintf(stdout, "Block number: %d\n", recBlockNumber);

        if (opCode == ERROR)
        {
            TftpErrorPacket errorPkt = *(TftpErrorPacket *)&dataPkt;
            fprintf(stderr, "Error: %s\n", errorPkt.errorMsg);
            break;
        }

        if (recBlockNumber != expectedBlock)
        {
            unexpectedBlocks++;
            fprintf(stderr, "Unexpected block number: %d\n", recBlockNumber);
            ackPkt.opcode = htons(ACK);
            ackPkt.blockNumber = htons(lastAckBlock);
            sendto(sockfd, &ackPkt, sizeof(ackPkt), 0, (const struct sockaddr *)server_addr, sizeof(*server_addr));

            if (unexpectedBlocks >= MAX_RETRIES)
            {
                fprintf(stderr, "Max unexpected blocks reached\n");
                break;
            }

            // try to get again the correct block
            int innerRetries = 0;
            while (innerRetries < MAX_RETRIES)
            {
                n = recvfrom(sockfd, &dataPkt, sizeof(dataPkt), 0, NULL, NULL);
                if (n < 0)
                {
                    if (errno == EAGAIN || errno == EWOULDBLOCK)
                    {
                        fprintf(stderr, "Timeout, retrying again...\n");
                        ackPkt.opcode = htons(ACK);
                        ackPkt.blockNumber = htons(lastAckBlock);
                        sendto(sockfd, &ackPkt, sizeof(ackPkt), 0, (const struct sockaddr *)server_addr, sizeof(*server_addr));
                        innerRetries++;
                        continue;
                    }
                    else
                    {
                        perror("Receive failed (inner retries)");
                        goto cleanup;
                    }
                }

                uint16_t innerBlockNumber = ntohs(dataPkt.blockNumber);
                if (innerBlockNumber == expectedBlock)
                {
                    recBlockNumber = innerBlockNumber;
                    break;
                }
                else
                {
                    fprintf(stderr, "Unexpected block number: %d, trying again\n", innerBlockNumber);
                    innerRetries++;
                }
            }
            if (innerRetries >= MAX_RETRIES)
            {
                fprintf(stderr, "Max unexpected blocks reached\n");
                break;
            }
            unexpectedBlocks = 0;
        }


        lastAckBlock = recBlockNumber;

        int data_len = n - 4;

        if (strncmp(mode, "netascii", 8) == 0)
        {
            // Convert data to ASCII
            for (int i = 0; i < data_len; i++)
            {
                if (dataPkt.data[i] == '\r')
                {
                    if (i + 1 < data_len)
                    {
                        if (dataPkt.data[i + 1] == '\n')
                        {
                            fputc('\n', file);
                            i++;
                        }
                        else if (dataPkt.data[i + 1] == '\0')
                        {
                            fputc('\r', file);
                            i++;
                        }
                        else
                        {
                            fputc(dataPkt.data[i], file);
                        }
                        
                    }
                    else
                    {
                        fputc('\r', file);
                    }                   
                }
                else
                {
                    fputc(dataPkt.data[i], file);
                }
            }
        }
        else if (strncmp(mode, "octet", 5) == 0)
        {
            // Write data to file
            if (fwrite(dataPkt.data, 1, data_len, file) != data_len)
            {
                perror("Write failed");
                break;
            }
        }
        else
        {
            fprintf(stderr, "Invalid transfer mode: %s\n", mode);
            break;
        }
        
        ackPkt.opcode = htons(ACK);
        ackPkt.blockNumber = htons(recBlockNumber);

        // Send ACK packet
        if (sendto(sockfd, &ackPkt, sizeof(ackPkt), 0, (const struct sockaddr *)server_addr, sizeof(*server_addr)) < 0)
        {
            perror("ACK send failed");
            break;
        }

        if (data_len < MAX_DATA_SIZE)
        {
            break;
        }
        expectedBlock++;
    }

cleanup:
    fclose(file);
    close(sockfd);
}

void set_file(char * mode, struct sockaddr_in *server_addr)
{
    // handle set command
    char src[MAX_FILENAME_SIZE];
    char dest[MAX_FILENAME_SIZE];

    // retrieve source and destination filenames from stdin
    scanf("%255s %255s", src, dest);
    fprintf(stdout, "src: %s, dest: %s\n", src, dest);

    // Create a UDP socket
    int sockfd = create_sockfd();
    set_socket_timeout(sockfd);

    TftpRequestPacket requestPacket;
    requestPacket.opcode = htons(WRQ);

    // set filename and mode
    strncpy(requestPacket.filename, dest, MAX_FILENAME_SIZE);
    requestPacket.filename[MAX_FILENAME_SIZE - 1] = '\0';
    strncpy(requestPacket.mode, mode, MAX_MODE_SIZE);
    requestPacket.mode[MAX_MODE_SIZE - 1] = '\0';

    FILE * file;
    if (strncmp(mode, "netascii", 8) == 0)
    {
        file = fopen(src, "rt");
    }
    else
    {
        file = fopen(src, "rb");
    }
    if(!file)
    {
        perror("File open failed");
        close(sockfd);
        return;
    }

    TftpDataPacket dataPkt;
    TftpAckPacket ackPkt;

    // Send message to server
    if (sendto(sockfd, &requestPacket, sizeof(requestPacket), 0, (const struct sockaddr *)server_addr, sizeof(*server_addr)) < 0)
    {
        perror("Request sending failed");
        close(sockfd);
        fclose(file);
        exit(EXIT_FAILURE);
    }

    int n = recvfrom(sockfd, &ackPkt, sizeof(ackPkt), 0, NULL, NULL);
    if (n < 0)
    {
        perror("Receive ACK from Request failed");
        fclose(file);
        close(sockfd);
        exit(EXIT_FAILURE);
    }

    if(ntohs(ackPkt.opcode) == ERROR)
    {
        TftpErrorPacket errorPkt = *(TftpErrorPacket *)&ackPkt;
        fprintf(stderr, "Error: %s\n", errorPkt.errorMsg);
        fclose(file);
        close(sockfd);
        exit(EXIT_FAILURE);
    }
    if (ntohs(ackPkt.blockNumber) != 0)
    {
        fprintf(stderr, "Invalid block number: %d\n", ntohs(ackPkt.blockNumber));
        fclose(file);
        close(sockfd);
        exit(EXIT_FAILURE);
    }

    uint16_t blockNumber = 1;
    int bytesRead = 0;

    printf("Uploading file %s...\n", src);

    // Receive response from server
    while (1)
    {
        bytesRead = fread(dataPkt.data, 1, MAX_DATA_SIZE, file);
        fprintf(stdout, "Read %d bytes from file\n", bytesRead);
        fprintf(stdout, "Block number: %d\n", blockNumber);

        dataPkt.opcode = htons(DATA);
        dataPkt.blockNumber = htons(blockNumber);

        // Data + header length
        int send_len = bytesRead + 4;

        // Send DATA packet
        if (sendto(sockfd, &dataPkt, send_len, 0, (const struct sockaddr *)server_addr, sizeof(*server_addr)) < 0)
        {
            perror("Data send failed");
            break;
        }

        // Receive ACK packet
        int retries = 0;
        int innerRetries = 0;
        while (retries < MAX_RETRIES)
        {
            n = recvfrom(sockfd, &ackPkt, sizeof(ackPkt), 0, NULL, NULL);
            if (n < 0)
            {
                if (errno == EAGAIN || errno == EWOULDBLOCK)
                {
                    fprintf(stderr, "Timeout, retrying again...\n");
                    if (sendto(sockfd, &dataPkt, send_len, 0, (const struct sockaddr *)server_addr, sizeof(*server_addr)) < 0)
                    {
                        perror("Data send failed");
                        break;
                    }
                    retries++;
                    continue;
                }
                else
                {
                    perror("Receive ACK failed");
                    break;
                }
            }
            else
            {
                uint16_t recvAck = ntohs(ackPkt.blockNumber);
                if (recvAck == blockNumber)
                {
                    break;
                }
                else
                {
                    fprintf(stderr, "Incorrect block number: %d\n", ntohs(ackPkt.blockNumber));
                    innerRetries = 0;
                    while (innerRetries < MAX_RETRIES)
                    {
                        if (sendto(sockfd, &dataPkt, send_len, 0, (const struct sockaddr *)server_addr, sizeof(*server_addr)) < 0)
                        {
                            perror("Data re-send failed");
                            break;
                        }
                        n = recvfrom(sockfd, &ackPkt, sizeof(ackPkt), 0, NULL, NULL);
                        if (n < 0)
                        {
                            if (errno == EAGAIN || errno == EWOULDBLOCK)
                            {
                                fprintf(stderr, "Timeout, retrying again...\n");
                                innerRetries++;
                                continue;
                            }
                            else
                            {
                                perror("Receive ACK failed");
                                break;
                            }
                        }
                        recvAck = ntohs(ackPkt.blockNumber);
                        if (recvAck == blockNumber)
                        {
                            break;
                        }
                        else
                        {
                            fprintf(stderr, "Incorrect block number: %d\n", ntohs(ackPkt.blockNumber));
                            innerRetries++;
                        }
                    }
                    if (innerRetries >= MAX_RETRIES)
                    {
                        fprintf(stderr, "Max retries reached\n");
                        goto cleanup;
                    }
                    retries = 0;
                    break;
                }
            }
        }

        if (retries >= MAX_RETRIES)
        {
            fprintf(stderr, "Max retries reached\n");
            break;
        }

        blockNumber++;
        if (bytesRead == 0) // EOF
        {
            dataPkt.opcode = htons(DATA);
            dataPkt.blockNumber = htons(blockNumber);
            sendto(sockfd, &dataPkt, 4, 0, (const struct sockaddr *)server_addr, sizeof(*server_addr));
            break;
        }
        if (bytesRead < MAX_DATA_SIZE)
        {
            break;
        }

    }

cleanup:
    fclose(file);
    close(sockfd);
}

int main(int argc, char *argv[])
{
    char * modeFlag = "octet";
    struct sockaddr_in server_addr;
    char command[1024];

    // Set up server address default structure
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(SERVER_PORT);
    server_addr.sin_addr.s_addr = inet_addr(SERVER_IP);

    while (1)
    {
        print_help_menu();
        print_prompt();

        memset(command, 0, sizeof(command)); // Clear buffer
        scanf("%1023s", command);
        if (strncmp(command, "help", 4) == 0)
        {
            // menu print help message
            continue;
        }
        else if (strncmp(command, "quit", 4) == 0)
        {
            // quit
            printf("Quitting...\n");
            break;
        }
        else if (strncmp(command, "mode", 4) == 0)
        {
            // handle mode command
            modeFlag = set_transfer_mode();
        }
        else if (strncmp(command, "ip", 2) == 0)
        {
            // handle ip command
            set_ip(&server_addr);
        }
        else if (strncmp(command, "port", 4) == 0)
        {
            // handle port command
            set_port(&server_addr);
        }
        else if (strncmp(command, "del", 3) == 0)
        {
            // handle del command
            del_file(&server_addr);
        }
        else if (strncmp(command, "get", 3) == 0)
        {
            // handle get command
            get_file(modeFlag, &server_addr);
        }
        else if (strncmp(command, "set", 3) == 0)
        {
            set_file(modeFlag, &server_addr);
        }
        else
        {
            // handle other commands if needed
            fprintf(stderr, "Invalid command: %s\n", command);
        }
    }

    return 0;
}