# TFTP-like UDP File Transfer System

This project implements a file transfer system over UDP, similar to TFTP, with enhanced features such as packet acknowledgment.

## Structure

*    **Server**: Implements functions for handling file read (download), write (upload), and deletion requests.
*    **Client**: Allows users to issue commands to upload, download, or delete files on the server.

## Compilation

Compile the server and client using GCC (or any C compiler). For example:
```bash
gcc udp_server.c -o server
gcc udp_client.c -o client
```
Make sure the header files are located in the ` includes/ ` folder (or adjust include paths accordingly).


## Running the Server

### Usage
```bash
./server [port] [base_directory]
```
* port: (Optional) The UDP port on which the server will listen. Default is 8080.
* base_directory: (Optional) The directory in which files are stored on the server. Default is /.

### Examples
* To run the server on the default port and base directory:
```bash
./server
```

* To run the server on port 9000 and use the base directory:
```bash
./server 9000
```

* To run the server on port 9000 and use /home/user/files as the base directory:
```bash
./server 9000 /home/user/files
```

### Behavior
* The server listens for incoming UDP requests (RRQ for downloads, WRQ for uploads, and DEL for file deletion).
* Upon receiving a request, the server processes it according to the TFTP protocol and sends the appropriate responses.
* The server prints status messages (e.g., requested file name, block numbers, errors) to the console.


## Running the Client

### Usage
```bash
./client
```
The client, when started, will display a help menu with available commands.

### Available Commands
* **Settings:**
* `mode <mode>`
  Sets the transfer mode. Use <txt> for text (netascii) mode or <bin> for binary (octet) mode. The default is binary.
* `ip <ip>`
  Sets the server IP address. Default is ` 127.0.0.1 `
* `port <port>`
  Sets the server port. Default is ` 8080 `

* **Commands:**
* `del <filename>`
  Sends a delete request to remove the specified file from the server.
* `get <src> <dest>`
  Downloads the file from the server: ` <src> ` is the source file name on the server, and ` <dest> ` is the destination file name on your machine.
* `set <src> <dest>`
  Uploads a file to the server: ` <src> ` is the file on your machine, and ` <dest> ` is the name under which it will be saved on the server

* **Other:**
* `help`
  Displays the help menu
* `quit`
  Exits the client application 

### Examples
* To download a file named ` example.txt ` from the server and save it as ` local_example.txt `:
```
get example.txt local_example.txt
```
* To upload a file named ` upload.txt ` to the server and save it as ` server_upload.txt `:
```
set upload.txt server_upload.txt
```
* To delete a file named ` oldfile.txt ` from the server:
```
del oldfile.txt
```

## Additional Notes
* **Timeouts & Retries:**
  Both client and server implement a retry mechanism for lost packets using a timeout of 3 seconds and a maximum number of retries defined by ` MAX_RETRIES `.
* **Transfer Modes:**
  The system supports two modes:
* **netascii (text mode):** Converts data to ASCII during transfer.
* **octet (binary mode):** Transfers raw binary data.
* **Error Handling:**
  In case of file not found, access violations, or illegal TFTP operations, error packets are sent and printed to the console.
