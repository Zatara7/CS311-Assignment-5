////////////////////////////////////////////////////////////////////////////////
//
//  File          : smsa_client.c
//  Description   : This is the client side of the SMSA communication protocol.
//
//   Author        : Hayder Sharhan
//   Last Modified : Sat Dec 7 2013
//

// Include Files
#include <stdio.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <string.h>
#include <unistd.h>

// Project Include Files
#include <smsa_network.h>
#include <smsa.h>

// Global variables
int server_socket;

// Functional Prototypes
int client_connect();
int client_disconnect();
int receive_packet(int, uint32_t *, int16_t *, int *, unsigned char *);
int send_packet(int, uint32_t, int16_t, unsigned char *);
int read_bytes(int, int, unsigned char *);
int wait_read(int);

//
// Functions

////////////////////////////////////////////////////////////////////////////////
//
// Function     : smsa_client_operation
// Description  : This the client operation that sends a reques to the SMSA
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
	// Things needed when receiving packet
	int16_t ret = 0;						// Return 0 or -1
	int blkbytes;							// How many bytes left
	uint32_t rop;							// Return operation

	if ( SMSA_OPCODE(op) == 0x0 ) {					// If mount
		if ((server_socket = client_connect()) == -1) {
			return -1;
		}
	}
       

	if ( send_packet( server_socket, op, 0, block ) == -1 ) {
		return( -1 );
	}
	
	/*if ( wait_read(server_socket) == -1 ) {
		return -1;
	}*/

	if ( receive_packet( server_socket, &rop, &ret, &blkbytes, block ) == -1 ) {
		return( -1 );
   	}
		
    	// Now check the op code
    	if ( op != rop ) {
		return( -1 );
    	}
	
	if ( SMSA_OPCODE(op) == 0x1 ) {				// If unmount
		if (client_disconnect() == -1) {
			return -1;
		}
    	}
	return ret;
}

int client_connect() {
	int sock;
	struct sockaddr_in my_struct;
	my_struct.sin_family = AF_INET;
	my_struct.sin_port = htons(SMSA_DEFAULT_PORT);
	
	if ((inet_aton(SMSA_DEFAULT_IP, &(my_struct.sin_addr))) == 0) {
		return -1;
	}
	
	sock = socket(AF_INET, SOCK_STREAM, 0);
	if (connect(sock, (const struct sockaddr *) &my_struct, sizeof(struct sockaddr)) == -1) {
		return -1;
	}

	return sock;
}

int client_disconnect() {
	if (close(server_socket) == -1) {
		return -1;
	}
	
	server_socket = -1;

	return 0;
}

int send_packet(int socket, uint32_t op, int16_t ret, unsigned char *block) {
	uint16_t len;
	unsigned char buf[SMSA_NET_HEADER_SIZE+SMSA_BLOCK_SIZE];
	int sb = 0;		// Sent Bytes
	int rb = 0;		// Returned Bytes
	
	// Only send block if write
	len = SMSA_NET_HEADER_SIZE;
	if (block != NULL) {
		len += SMSA_BLOCK_SIZE;
	}
	
	len = htons(len);
	op = htonl(op);
	ret = htons(ret);
	
	// Copy data into buffer
	memcpy( &buf[0], &len, sizeof(len)); // Copy length
	memcpy( &buf[2], &op, sizeof(op)); 	 // Copy op
	memcpy( &buf[6], &ret, sizeof(ret));  // Copy Return Value
	
	// Only add block to buffer if reading
	if (block != NULL) {
		memcpy(&buf[8], block, SMSA_BLOCK_SIZE); // Copy block
		
		while (sb < 263) {
			if ((rb = write(socket, &buf[sb], 263-sb)) <= 0) {
				return -1;
			}
		
			sb += rb;
		}
		
		return 0;
	}
	
	while (sb < 7) {
		if ((rb = write(socket, &buf[sb], 7-sb)) <= 0) {
			return -1;
		}
		
		sb += rb;
	}
	
	return 0;
}

int receive_packet(int socket, uint32_t *op, int16_t *ret, int *blkbytes, unsigned char *block) {
	uint16_t len;
	unsigned char buf[SMSA_NET_HEADER_SIZE];
	
	if (read_bytes(socket, SMSA_NET_HEADER_SIZE, buf ) == -1) {
		return -1;
	}
	printf("after bytes read\n");
	
	memcpy( &len, buf, sizeof(uint16_t));
	len = ntohs(len);
	memcpy( op, &buf[2], sizeof(uint32_t));
	*op = ntohl(*op);
	memcpy( ret, &buf[6], sizeof(int16_t));
	*ret = ntohs(*ret);
	
	if ( len > SMSA_NET_HEADER_SIZE ) {
		if ( read_bytes(socket, len-SMSA_NET_HEADER_SIZE, block) == -1) {
			return -1;
		}
		*blkbytes = len-SMSA_NET_HEADER_SIZE;
	} else {
		*blkbytes = 0;
	}
	
	return 0;	
}

int read_bytes(int socket, int len, unsigned char *buf) {
	int read_bytes = 0;
	int rtnd_bytes = 0;
	
	printf("inside read bytes\n");

	while (read_bytes < len) {
		if ((rtnd_bytes = read(socket, &buf[read_bytes], len-read_bytes)) <= 0) {
			return -1;
		}
		printf("inside while loop\n");
		read_bytes += rtnd_bytes;
	}
	printf("after while loop\n");

	return 0;
}

int wait_read( int sock ) {

    // Local variables
    fd_set rfds;
    int nfds, ret;
	
    printf("before setup\n");
    // Setup and perform the select
    nfds = sock + 1;
    FD_ZERO( &rfds );
    FD_SET( sock, &rfds );
    ret = select( nfds, &rfds, NULL, NULL, NULL );
    printf("after setup and select\n");
    // Check the return value
    if ( ret == -1 ) {
	return( -1 );
    }

    // check to make sure we are selected on the read
    if ( FD_ISSET( sock, &rfds ) == 0 ) {
	return( -1 );
    }

    // Return successsfully
    return( 0 );
}
