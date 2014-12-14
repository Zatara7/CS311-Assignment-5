////////////////////////////////////////////////////////////////////////////////
//
//  File          : smsa_client.c
//  Description   : This is the client side of the SMSA communication protocol.
//
//   Author        : Hayder Sharhan
//   Last Modified : Tue Dec 10 2013
//

// Include Files
#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <string.h>
#include <unistd.h>

// Project Include Files
#include <smsa_network.h>
#include <smsa.h>

// Global variables
int server_socket = -1;

// Functional Prototypes
int client_connect();
int client_disconnect();
int send_packet( int, uint32_t, unsigned char * );
int receive_packet( int, uint32_t *, int16_t *, unsigned char * ); 

////////////////////////////////////////////////////////////////////////////////
//
// Function     : smsa_client_operation
// Description  : This the client operation that sends a request to the SMSA
//                server.   It will:
//
//                1) if mounting make a connection to the server 
//                2) send any request to the server, returning results
//                3) if unmounting, will close the connection
//
// Inputs       : op - the operation code for the command
//                block - the block to be read/writen from (READ/WRITE)
// Outputs      : 0 if successful, -1 if failure

int smsa_client_operation( uint32_t op, unsigned char *block ) {
	int16_t ret;
	uint32_t rop;

	if ( SMSA_OPCODE(op) == 0x0 ) {					// If mount
		if ( (server_socket = client_connect()) == -1 ) {	// Connect to client
			return -1;
		}
	}

	if ( send_packet( server_socket, op, block ) == -1 ) {	// Send info to server
		return -1;
	}

	if ( receive_packet( server_socket, &rop, &ret, block ) == -1 ) { // Receive info from server
		return -1;
	}

	if ( op != rop ) {						// Check if info received is the same
		return -1;
	}

	if ( SMSA_OPCODE(op) == SMSA_UNMOUNT ) {			// Check if unmount
		if (client_disconnect() == -1) {			// Disconnect from server
			return -1;
		}
	}

	return ret;							// Return what the server returned
}

////////////////////////////////////////////////////////////////////////////////
//
// Function     : client_connect
// Description  : This function will connect to a SMSA server at SMSA_DEFAUL_IP 
// 		and SMSA_DEFAULT_PORT and return a socket for the connection.
//
// Inputs       : none
// Outputs      : socket if successful, -1 if failure

int client_connect() {
	int sock;
	struct sockaddr_in my_struct;

	// Prepare connection information
	my_struct.sin_family = AF_INET;
	my_struct.sin_port = htons(SMSA_DEFAULT_PORT);
	if ( inet_aton( (char *) SMSA_DEFAULT_IP, &(my_struct.sin_addr) ) == 0 ) {
		return -1;
	}
	
	// Receive a proper socket
	if ( ( sock = socket(AF_INET, SOCK_STREAM, 0) ) == -1 ) {
		return -1;
	}
	
	// Send prepared information to server and establish a connection
	if ( connect( sock, (const struct sockaddr *) &my_struct, sizeof(struct sockaddr) ) == -1 ) {
		return -1;
	}

	return sock;
}

////////////////////////////////////////////////////////////////////////////////
//
// Function     : client_disconnect
// Description  : This function will disconnect the client from the server
// 			using the server_socket socket.
//
// Inputs       : none
// Outputs      : 0 if successful, -1 if failure

int client_disconnect() {
	if (close( server_socket ) == -1) {	// Close the server connection
		return -1;
	}
	server_socket = -1;			// Good Practice

	return 0;
}

////////////////////////////////////////////////////////////////////////////////
//
// Function     : send_packet
// Description  : This function will send 
//
// Inputs       : int sock - socket to the server connection.
// 		  uint32_t op - opcode of the operation to be sent.
// 		  unsigned char *block - block to be sent to the server.
// Outputs      : 0 if successful, -1 if failure

int send_packet( int sock, uint32_t op, unsigned char *block ) {
	uint16_t len;			// Length of our package
	unsigned char hdr[264];		// To store package data
	uint16_t ret = 0;

	len = SMSA_NET_HEADER_SIZE;	// Initialize package to be only HDR size (8)

	if ( block != NULL ) {		// If we are using the block i.e. writing
		len += SMSA_BLOCK_SIZE; // Then increase the length of the package
	}
	
	// Put data in network format
	len = htons(len);
	op = htonl(op);
	ret = htons(ret);

	// Copy the data into the array were sending
	memcpy( &hdr[0], &len, 2);
	memcpy( &hdr[2], &op, 4);
	memcpy( &hdr[6], &ret, 2);

	if ( block != NULL ) {					// If we are using the block
		memcpy( &hdr[8], block, SMSA_BLOCK_SIZE );	// Copy the block into our array

		if( write(sock, &hdr[0], 264) == -1) {
			return -1;
		}
		return 0;
	}

	if ( write(sock, &hdr[0], 8) == -1) {
		return -1;
	}

	return 0;
}

////////////////////////////////////////////////////////////////////////////////
//
// Function     : receive_packet
// Description  : This function will receive the data from the server
//
// Inputs       : int sock - socket used for connection.
// 		  uint32_t *op - opcode to be received. (by reference)
// 		  int16_t *ret - what the server returns. (by reference)
// 		  unsigned char *block - The block to be received rom the server.
// 		  			(if needed)
// Outputs      : 0 if successful, -1 if failure

int receive_packet( int sock, uint32_t *op, int16_t *ret, unsigned char *block ) {
	uint16_t len;					// To store the length
	unsigned char hdr[SMSA_NET_HEADER_SIZE];	// To store the received array
	

	// Receive data into our array from the server
	if ( read( sock, &hdr[0], SMSA_NET_HEADER_SIZE ) == -1 ) {
		return -1;
	}

	// Decompose the array into the needed variables and put in network format
	memcpy( &len, hdr, 2);
	len = ntohs( len );
	memcpy( op, &hdr[2], 4);
	*op = ntohl( *op );
	memcpy( ret, &hdr[6], 2);
	*ret = ntohs(*ret);

	if ( len > SMSA_NET_HEADER_SIZE ) {				// If len is larger than 8 (i.e. read) then we must
		if ( read( sock, &block[0], SMSA_BLOCK_SIZE ) == -1 ) {	// Obtain the block too
			return -1;
		}
		return 0;
	}

	return 0;
}
