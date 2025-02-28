#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <dirent.h>
#include <errno.h>
#include <sys/time.h>
#include "Includes/udp_server.h"

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

// Create a UDP socket
int create_udp_socket(int port)
{
    int sockfd;
    struct sockaddr_in server_addr;
    struct timeval timeout;

    // Create a UDP socket
    if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0)
    {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }

    // Set up server address structure
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(port);

    // Bind the socket to the server address
    if (bind(sockfd, (const struct sockaddr *)&server_addr, sizeof(server_addr)) < 0)
    {
        perror("Bind failed");
        close(sockfd);
        exit(EXIT_FAILURE);
    }

    printf("UDP server listening on port %d...\n", port);

    return sockfd;
}

// Send file in octet or netascii mode
void send_file(int sockfd, TftpRequestPacket *req, struct sockaddr_in *client_addr, socklen_t addr_len, char *base_dir)
{
    char filename[512];

    // Set up file path
    snprintf(filename, 512, "%s/%s", base_dir, req->filename);
    fprintf(stdout, "Client requested file: %s (mode: %s)\n", req->filename, req->mode);

    FILE *file;

    if (strncmp(req->mode, "octet", 5) == 0)
    {
        file = fopen(filename, "rb");
    }
    else if (strncmp(req->mode, "netascii", 8) == 0)
    {
        file = fopen(filename, "rt");
    }
    else
    {
        fprintf(stderr, "Invalid transfer mode: %s\n", req->mode);
        return;
    }

    if (!file)
    {
        TftpErrorPacket errPkt;
        errPkt.opcode = htons(ERROR);
        errPkt.errorCode = htons(1); // File not found
        strncpy(errPkt.errorMsg, "File not found", MAX_ERROR_MSG_SIZE);
        sendto(sockfd, &errPkt, sizeof(errPkt), 0, (const struct sockaddr *)client_addr, addr_len);
        perror("File open failed");
        return;
    }

    TftpDataPacket dataPkt;
    int blockNumber = 1;
    size_t bytesRead;

    fprintf(stdout, "Downloading file %s...\n", filename);

    // Read file and send data to client
    while (1)
    {
        bytesRead = fread(dataPkt.data, 1, MAX_DATA_SIZE, file);
        fprintf(stdout, "Read %ld bytes from file\n", bytesRead);
        fprintf(stdout, "Block number: %d\n", blockNumber);

        dataPkt.opcode = htons(DATA);
        dataPkt.blockNumber = htons(blockNumber);

        // Data + header length
        size_t send_len = bytesRead + 4;

        // Send DATA packet
        if (sendto(sockfd, &dataPkt, send_len, 0, (const struct sockaddr *)client_addr, addr_len) < 0)
        {
            perror("Data send failed");
            break;
        }

        // Receive ACK packet
        TftpAckPacket ackPkt;
        int retries = 0;
        int n;
        while (retries < MAX_RETRIES)
        {
            n = recvfrom(sockfd, &ackPkt, sizeof(ackPkt), 0, NULL, NULL);
            if (n < 0)
            {
                if (errno == EAGAIN || errno == EWOULDBLOCK)
                {
                    fprintf(stderr, "Timeout, retrying...\n");
                    if (sendto(sockfd, &dataPkt, send_len, 0, (const struct sockaddr *)client_addr, addr_len) < 0)
                    {
                        perror("Data send failed");
                        break;
                    }
                    retries++;
                    continue;
                }
                perror("Receive ACK failed");
                break;
            }
            if (ntohs(ackPkt.blockNumber) == blockNumber)
            {
                break;
            }
            fprintf(stderr, "Invalid block number: %d\n", ntohs(ackPkt.blockNumber));
            retries++;
        }   
        if (retries >= MAX_RETRIES)
        {
            fprintf(stderr, "Max retries reached\n");
            break;
        }

        if (bytesRead == 0) // EOF
        {
            dataPkt.opcode = htons(DATA);
            dataPkt.blockNumber = htons(blockNumber + 1);
            if (sendto(sockfd, &dataPkt, 4, 0, (const struct sockaddr *)client_addr, addr_len) < 0)
            {
                perror("Data send failed");
                break;
            }
            break;
        }

        if (bytesRead < MAX_DATA_SIZE)
        {

            break;
        }
        blockNumber++;

    }

    fclose(file);
}

// Set file in octet or netascii mode
void set_file(int sockfd, TftpRequestPacket *req, struct sockaddr_in *client_addr, socklen_t addr_len, char *base_dir)
{
    char filename[512];

    // Set up file path
    snprintf(filename, 512, "%s/%s", base_dir, req->filename);
    fprintf(stdout, "Client requested to set file: %s (mode: %s)\n", req->filename, req->mode);

    FILE *file;

    if (strncmp(req->mode, "octet", 5) == 0)
    {
        file = fopen(filename, "wb");
    }
    else if (strncmp(req->mode, "netascii", 8) == 0)
    {
        file = fopen(filename, "wt");
    }
    else
    {
        fprintf(stderr, "Invalid transfer mode: %s\n", req->mode);
        return;
    }

    if (!file)
    {
        TftpErrorPacket errPkt;
        errPkt.opcode = htons(ERROR);
        errPkt.errorCode = htons(2); // Access violation
        strncpy(errPkt.errorMsg, "Can't open file", MAX_ERROR_MSG_SIZE);
        sendto(sockfd, &errPkt, sizeof(errPkt), 0, (const struct sockaddr *)client_addr, addr_len);
        perror("File open failed");
        return;
    }

    TftpAckPacket ackPkt;
    ackPkt.opcode = htons(ACK);
    ackPkt.blockNumber = htons(0);

    // Send ACK packet
    if (sendto(sockfd, &ackPkt, sizeof(ackPkt), 0, (const struct sockaddr *)client_addr, addr_len) < 0)
    {
        perror("ACK send failed");
        fclose(file);
        return;
    }

    TftpDataPacket dataPkt;
    uint16_t blockNumber = 1;
    int n;

    fprintf(stdout, "Uploading file %s...\n", filename);

    // Receive data and write to file
    while (1)
    {
        // Receive DATA packet
        n = recvfrom(sockfd, &dataPkt, sizeof(dataPkt), 0, NULL, NULL);
        if (n < 0)
        {
            if (errno == EAGAIN || errno == EWOULDBLOCK)
            {
                fprintf(stderr, "Timeout, retrying...\n");
                if (sendto(sockfd, &ackPkt, sizeof(ackPkt), 0, (const struct sockaddr *)client_addr, addr_len) < 0)
                {
                    perror("ACK send failed");
                    break;
                }
                continue;
            }
            else
            {
                perror("Receive failed");
                break;
            }
        }

        uint16_t recOpCode = ntohs(dataPkt.opcode);
        uint16_t recBlockNumber = ntohs(dataPkt.blockNumber);

        fprintf(stdout, "Received %d bytes\n", n);
        fprintf(stdout, "Block number: %d\n", recBlockNumber);

        if (recOpCode == ERROR)
        {
            TftpErrorPacket * errorPkt = (TftpErrorPacket *)&dataPkt;
            fprintf(stderr, "Error from client: %s\n", errorPkt->errorMsg);
            break;
        }
        if (recOpCode != DATA)
        {
            TftpErrorPacket errPkt;
            errPkt.opcode = htons(ERROR);
            errPkt.errorCode = htons(4); // Illegal TFTP operation
            strncpy(errPkt.errorMsg, "Illegal TFTP operation", MAX_ERROR_MSG_SIZE);
            sendto(sockfd, &errPkt, sizeof(errPkt), 0, (const struct sockaddr *)client_addr, addr_len);
            fprintf(stderr, "Invalid opcode: %d\n", recOpCode);
            break;
        }
        if (recBlockNumber != blockNumber)
        {
            TftpErrorPacket errPkt;
            errPkt.opcode = htons(ERROR);
            errPkt.errorCode = htons(0); // Undefined error
            strncpy(errPkt.errorMsg, "Invalid block number", MAX_ERROR_MSG_SIZE);
            sendto(sockfd, &errPkt, sizeof(errPkt), 0, (const struct sockaddr *)client_addr, addr_len);
            fprintf(stderr, "Invalid block number: %d\n", recBlockNumber);
            break;
        }

        int data_len = n - 4; // Data length

        if (strncmp(req->mode, "netascii", 8) == 0)
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
        else
        {
            // Write data to file
            if (fwrite(dataPkt.data, 1, data_len, file) != data_len)
            {
                perror("Write failed");
                break;
            }
        }

        ackPkt.opcode = htons(ACK);
        ackPkt.blockNumber = htons(blockNumber);
        if (sendto(sockfd, &ackPkt, sizeof(ackPkt), 0, (const struct sockaddr *)client_addr, addr_len) < 0)
        {
            perror("ACK send failed");
            break;
        }

        if (data_len < MAX_DATA_SIZE)
        {
            break;
        }
        
        blockNumber++;

    }

    fclose(file);

}

// Delete file
void delete_file(int sockfd, TftpDeletePacket *delPkt, struct sockaddr_in *client_addr, socklen_t addr_len, char *base_dir)
{
    char filename[512];

    // Set up file path
    snprintf(filename, 512, "%s/%s", base_dir, delPkt->filename);
    fprintf(stdout, "Client requested to delete file: %s\n", delPkt->filename);

    if (remove(filename) == 0)
    {
        fprintf(stdout, "File %s deleted successfully.\n", delPkt->filename);
        char response[] = "File deleted successfully.";
        sendto(sockfd, response, sizeof(response), 0, (const struct sockaddr *)client_addr, addr_len);
    }
    else
    {
        char response[BUFFER_SIZE];
        snprintf(response, BUFFER_SIZE, "File delete failed: %s", strerror(errno));
        sendto(sockfd, response, sizeof(response), 0, (const struct sockaddr *)client_addr, addr_len);
        perror("File delete failed");
    }
}

// Start listening for client requests
void listen_for_requests(int sockfd, char *base_dir)
{
    char buffer[BUFFER_SIZE];
    struct sockaddr_in client_addr;
    socklen_t addr_len = sizeof(client_addr);

    while (1)
    {

        memset(buffer, 0, BUFFER_SIZE); // Clear buffer

        fprintf(stdout, "Waiting for client request...\n");

        // Receive message from client
        int n = recvfrom(sockfd, buffer, BUFFER_SIZE, 0, (struct sockaddr *)&client_addr, &addr_len);
        if (n < 0)
        {
            perror("Receive failed");
            continue;
        }

        uint16_t opcode = ntohs(*(uint16_t *)buffer);
        set_socket_timeout(sockfd);

        switch (opcode)
        {
            case RRQ: // Read Request
            {
                TftpRequestPacket *req = (TftpRequestPacket *)buffer;
                send_file(sockfd, req, &client_addr, addr_len, base_dir);
                return;
            }

            case WRQ: // Write Request
            {
                TftpRequestPacket *req = (TftpRequestPacket *)buffer;
                set_file(sockfd, req, &client_addr, addr_len, base_dir);
                return;
            }

            case DEL: // Delete Request
            {
                TftpDeletePacket *delPkt = (TftpDeletePacket *)buffer;
                delete_file(sockfd, delPkt, &client_addr, addr_len, base_dir);
                return;
            }
            default: // Invalid opcode
            {
                TftpErrorPacket errPkt;
                errPkt.opcode = htons(ERROR);
                errPkt.errorCode = htons(4); // Illegal TFTP operation
                strncpy(errPkt.errorMsg, "Illegal TFTP operation", MAX_ERROR_MSG_SIZE);
                sendto(sockfd, &errPkt, sizeof(errPkt), 0, (const struct sockaddr *)&client_addr, addr_len);
                fprintf(stderr, "Invalid opcode: %d\n", opcode);
                return;
            }
        }
    }
}

int main(int argc, char *argv[])
{
    fprintf(stdout, "Starting UDP server...\n\n"); 
    char base_dir[512] = "/";
    int port = 8080;

    // check for arguments
    if (argc == 3)
    {
        port = atoi(argv[1]);
        strncpy(base_dir, argv[2], 512);
    }
    else if (argc == 2)
    {

        port = atoi(argv[1]);
    }
    else if (argc != 1)
    {
        fprintf(stderr, "Usage:\n\t%s <port> <base dir>\n\t%s <port>\n\t%s\n\nDefault:\n\tport: 8080\n\tdir: /\n\n", argv[0], argv[0], argv[0]);
        exit(EXIT_FAILURE);
    }

    fprintf(stdout, "Port number set to %d.\nBase dir set to %s.\n", port, base_dir);
    DIR *dir = opendir(base_dir);
    if (dir)
    {
        closedir(dir);
    }
    else if (ENOENT == errno)
    {
        fprintf(stderr, "Directory %s does not exist.\n", base_dir);
        exit(EXIT_FAILURE);
    }
    else
    {
        fprintf(stderr, "Error opening directory %s.\n", base_dir);
        exit(EXIT_FAILURE);
    }

    while (1)
    {
        int sockfd = create_udp_socket(port);
        if (sockfd < 0)
        {
            exit(EXIT_FAILURE);
        }

        listen_for_requests(sockfd, base_dir);
        close(sockfd);
    }

    return 0;
   
}