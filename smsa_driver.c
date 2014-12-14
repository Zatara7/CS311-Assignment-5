////////////////////////////////////////////////////////////////////////////////
//
//  File           : smsa_driver.c
//  Description    : This is the driver for the SMSA simulator.
//
//   Author        : Hayder Sharhan
//   Created 	   : Mon Oct 07 2013
//   Last Modified : Mon Dec 09 2013
//

// Include Files

// Project Include Files
#include <smsa_driver.h>
#include <cmpsc311_log.h>
#include <stdio.h> // not sure if needed
#include <smsa_network.h>

// Defines
// Functional Prototypes
// ~Defined in header file !
//
// Global data

// Interfaces

////////////////////////////////////////////////////////////////////////////////
//
// Function     : smsa_vmount
// Description  : Mount the SMSA disk array virtual address space
//
// Inputs       : none
// Outputs      : -1 if failure or 0 if successful

int smsa_vmount( uint32_t lines ) {
	uint32_t op = get_opcode( 0x0, 0, 0 );		// Mount
	if (smsa_client_operation( op, NULL ) == -1) { 	// Call smsa
		return -1;
	}
	
	/*if ( load_workload_file() == -1) {		// If there's a problem loading file
		return -1;				// Report problem
	}*/
	
	smsa_init_cache(lines);				// Initialize the cache

	return ( 0 );
}

////////////////////////////////////////////////////////////////////////////////
//
// Function     : smsa_vunmount
// Description  :  Unmount the SMSA disk array virtual address space
//
// Inputs       : none
// Outputs      : -1 if failure or 0 if successful

int smsa_vunmount( void )  {
	/*if (save_workload_file() == -1) { 	// If there's a problem saving file
		return -1;			// report problem
	}*/

	smsa_close_cache();			// Close the cache and free what's in it

	uint32_t op = get_opcode( 0x1, 0, 0 ); 	// Unmount the device
	if (smsa_client_operation( op, NULL ) == -1) {
		return -1;
	}
	
	return ( 0 );
}

////////////////////////////////////////////////////////////////////////////////
//
// Function     : smsa_vread
// Description  : Read from the SMSA virtual address space
//
// Inputs       : addr - the address to read from
//                len - the number of bytes to read
//                buf - the place to put the read bytes
// Outputs      : -1 if failure or 0 if successful

int smsa_vread( SMSA_VIRTUAL_ADDRESS addr, uint32_t len, unsigned char *buf ) {
	// Decompose address and check for failures
	int drm;
       	if ( (drm = get_current_drum( addr ) ) == -1) {
		return -1;
	}
	int blk;
       	if ( (blk = get_current_block( addr ) ) == -1) {
		return -1;
	}
	int off;
       	if ( (off = get_current_offset( addr ) ) == -1) {
		return -1;
	}
	int indc;							// An indicator for previous operation caching.
									// 0 for cached 1 for no cache


	// *Take care of edge case*
	unsigned char *tptr = NULL;
	tptr = smsa_get_cache_line(drm, blk);				// Look if the entry is cached

	if (tptr == NULL) {						// If it isn't
		smsa_client_operation(get_opcode(0x2, drm, blk), NULL);	// Seek drum
		smsa_client_operation(get_opcode(0x3, drm, blk), NULL);	// Seek block

		tptr = malloc(256);					// Request Heap memory
		smsa_client_operation(get_opcode(0x4, drm, blk), tptr); 	// Read into temp -> Will seek to next blk
		smsa_put_cache_line(drm, blk, tptr);			// Cache the data
		indc = 1;						// Update indicator for next step
	} else {
		smsa_client_operation(get_opcode(0x2, drm, blk), NULL);	// Only Seek for drum [This is handled in a weird way for optimization]
		indc = 0;						// Update inidcator
	}

	int i = off;						// index assigned to offset
	int rb = 0;						// To keep track of bytes read
	while (i < 256 && rb < len) {
		buf[rb] = tptr[i];				// fill buffer with temp's data
		i++;
		rb++;
	}
	blk++;							// Increment the block
	
	// *Took care of edge case*
	

	// Take care of the regular case
	while (rb < len) {						// Loop until the read bytes are less than the length of buffer
		if (blk == 256) {					// Check if we reach end of drum
			drm++;						// Increment drum
			if (drm > 15) {					// Check if we reach end of array
				return -1;
			}
			blk = 0;					// Reset blcok

			smsa_client_operation(get_opcode(0x2, drm, blk), NULL);// Seek to the new incremented drum
		}

		tptr = smsa_get_cache_line(drm, blk);			// Look if entry is cached	

		if (tptr == NULL) {					// If it isn't
			if (indc == 0 || blk == 0) {
				smsa_client_operation(get_opcode(0x3, drm, blk), NULL);// Seek block
			}
			tptr = malloc(256);					// Ask OS for 256 memory bytes to cache the block
			smsa_client_operation(get_opcode(0x4, drm, blk), tptr);	// Read into temp
			smsa_put_cache_line(drm, blk, tptr);			// cache the block
			indc = 1;						// Update indicator
		}	else {
			indc = 0;						// If it is update the incrementer
		}

		i = 0;							// Set i back to 0
		
		while (i < 256 && rb < len) {
			buf[rb] = tptr[i];				// Each byte in temp to the buffer
			i++;
			rb++;						// Increment our read bytes
		}
		
		blk++;							// Increment local block count
	}

	return ( 0 );
}

////////////////////////////////////////////////////////////////////////////////
//
// Function     : smsa_vwrite
// Description  : Write to the SMSA virtual address space
//
// Inputs       : addr - the address to write to
//                len - the number of bytes to write
//                buf - the place to read the read from to write
// Outputs      : -1 if failure or 0 if successful

int smsa_vwrite( SMSA_VIRTUAL_ADDRESS addr, uint32_t len, unsigned char *buf )  {
	// Decompose address and check for failures
	int drm;
       	if ( (drm = get_current_drum( addr ) ) == -1) {
		return -1;
	}
	int blk;
       	if ( (blk = get_current_block( addr ) ) == -1) {
		return -1;
	}
	int off;
       	if ( (off = get_current_offset( addr ) ) == -1) {
		return -1;
	}

	// * Take care of edge case *
	int i = off;						// Set the index to the block offset
	int wb = 0;						// To keep track of bytes written

	unsigned char *tptr = NULL;				

	tptr = smsa_get_cache_line(drm, blk);			// See if entry is cached

	if (tptr == NULL) {					// If it isn't
		smsa_client_operation(get_opcode(0x2, drm, blk), NULL);	// Seek to current drum
		smsa_client_operation(get_opcode(0x3, drm, blk), NULL); 	// Seek to current block

		tptr = malloc(256);					// Ask OS for 256 bytes in heap
		smsa_client_operation(get_opcode(0x4, drm, blk), tptr);	// Read into temp
		smsa_put_cache_line(drm, blk, tptr);			// Put the ptr into cache
		
		smsa_client_operation(get_opcode(0x3, drm, blk), NULL);	// Seek back to the block
	} else {
		smsa_client_operation(get_opcode(0x2, drm, blk), NULL);	// Seek to drum
		smsa_client_operation(get_opcode(0x3, drm, blk), NULL); 	// Seek to block
	}

	while (i < 256 && wb < len) {				// Loop until we read as many bytes as buffer length
		tptr[i] = buf[wb];				// Assign each byte in buffer to temp
		i++;
		wb++;						// Increment the count of written bytes
	}
	
	smsa_client_operation(get_opcode(0x5, drm, blk), tptr); 	// Dump data to smsa
	blk++;							// Increment local count of blocks
	// * Took care of edge case *
	

	// Take care of the regular rest
	while (wb < len) {
		if (blk == 256) { 				// Check if drum is filled up
			drm++;					// Step to next drum
			if (drm > 15) {				// Check if stepped off smsa
				return -1;
			}
			blk = 0; 				// Reset the block
			smsa_client_operation(get_opcode(0x2, drm, blk), NULL);	// Seek to the next drum
			smsa_client_operation(get_opcode(0x3, drm, blk), NULL);	// Seek to block 0
		}

		tptr = smsa_get_cache_line(drm, blk);		// Fetch drm and blk to see if they're in the cache
		
		if (tptr == NULL) {					// If they aren't, do this
			tptr = malloc(256);				// Obtain 256 data from memory
			smsa_client_operation(get_opcode(0x4, drm, blk), tptr);// backup the data into the ptr
			smsa_put_cache_line(drm, blk, tptr);		// Put data in the drum
			smsa_client_operation(get_opcode(0x3, drm, blk), NULL);// Seek back only for block
		}

		i = 0;						 // Reset index
		while ( i < 256 && wb < len ) {			 // Loop until we read enough bytes
			tptr[i] = buf[wb];			 // Assign each byte in buffer to temp
			i++;
			wb++;					 // Increment the amount of bytes read
		}

		smsa_client_operation(get_opcode(0x5, drm, blk), tptr); // Dump temp to smsa

		blk++;
	}

	return ( 0 );
}


////////////////////////////////////////////////////////////////////////////////
//
// Function     : get_current_drum
// Description  : Will return the drum referenced by the address
//
// Inputs       : addr - the address to decompose
// Outputs      : -1 if failure or the drum ID if successful

int get_current_drum( SMSA_VIRTUAL_ADDRESS addr ) {
	int dummy = (addr >> 16);		// Shift right by 16 to get the first four signnificant bits

	if (dummy > 15 || dummy < 0) {	// Check boundaries
		return -1;
	}

	return dummy;			// Return final value
}

////////////////////////////////////////////////////////////////////////////////
//
// Function     : get_current_block
// Description  : Will return the block referenced by the address
//
// Inputs       : addr - the address to decompose
// Outputs      : -1 if failure or the block ID if successful

int get_current_block( SMSA_VIRTUAL_ADDRESS addr ) {
	int dummy = addr & 0x0FF00; 	// Mask the address to get the first 16

	dummy = dummy >> 8;		// Shift right by 8 to get the middle 8
	
	if (dummy > 255 || dummy < 0) {	// Check boundaries
		return -1;
	}

	return dummy;			// Return final value
}

////////////////////////////////////////////////////////////////////////////////
//
// Function     : get_current_offset
// Description  : Will return the offset referenced by the address
//
// Inputs       : addr - the address to decompose
// Outputs      : -1 if failure or the offset if successful

int get_current_offset( SMSA_VIRTUAL_ADDRESS addr ) {
	int dummy = addr & 0xFF;	// Mask the address to obtain the first 8
	
	if (dummy > 255 || dummy < 0) {	// Check boundaries
		return -1;
	
	}

	return dummy;			// Return final value
}

////////////////////////////////////////////////////////////////////////////////
//
// Function     : get_opcode
// Description  : Will combine the three inputs into opcode that could 
// 			communicate with smsa.
//
// Inputs       : command - 0 through 9 instructions that tell smsa what to do
// 		  drumID - The drum ID if needed
// 		  blockID - the block ID if needed
// Outputs      : -1 if failure or funcitonal opcode if successful

uint32_t get_opcode( int command, int drumID, int blockID ) {
	if (command > 9 || command < 0) {	// Check boundaries
		return -1;
	} else {	
		command = command << 26;	// Shift to the first 6 bits
	}

	if (drumID < 0 || drumID > 15) {	// Check boundaries
		return -1;
	} else {
		drumID = drumID << 22;		// Shift to 7 to 10 bits
		command = command | drumID;	// Combine the two
	}
	if (blockID < 0 || blockID > 255) {	// Check boundaries
		return -1;
	} else {
		command = command | blockID;	// Combine to be the last 8 bits
	}

	return command;				// Return final value
}

////////////////////////////////////////////////////////////////////////////////
//
// Function     : load_workload_file
// Description  : Will load whatever is in the file smsa_data.dat to smsa.
//
// Inputs       : none.
// Outputs      : -1 if failure or 0 if success

int load_workload_file( void ) {
	FILE *fhandle = NULL; 				// Initialize all pointers to null :)
	if ((fhandle = fopen("smsa_data.dat", "rb")) == NULL) {		// open file
		printf("INFO: File smsa_data.dat does not exit : Continuing without loading\n");
		return ( 0 );
	}
	unsigned char temp[SMSA_BLOCK_SIZE]; 		// will use to load up files

	smsa_client_operation(get_opcode(0x2, 0, 0), NULL); 	// Seek drum 0
	smsa_client_operation(get_opcode(0x3, 0, 0), NULL); 	// Seek blck 0
	int blk = 0;
	int drm = 0;
	while (drm < 16) {
		fread(temp, sizeof(temp[0]), (sizeof(temp) / sizeof(temp[0])), fhandle);	// Read temp from file
		smsa_client_operation(get_opcode(0x5, drm, blk), temp); 				// Write temp to smsa
		blk++;										// Increment local block
		if (blk == 256) {								// Check if stepped off block
			drm++;									// Increment local drum
			if (drm > 15) {								// Check if stepped off array
				break;
			}
			blk = 0;								// reset block
			smsa_client_operation(get_opcode(0x2, drm, blk), NULL); 			// Seek to new drum
			smsa_client_operation(get_opcode(0x3, drm, blk), NULL); 			// Seek to new block
	 	}
	}

	fclose( fhandle );

	return ( 0 );
}

////////////////////////////////////////////////////////////////////////////////
//
// Function     : save_workload_file
// Description  : Will save whatever is currently in smsa to smsa_data.dat.
//
// Inputs       : none.
// Outputs      : -1 if failure or 0 if successful.

int save_workload_file ( void ) {
	FILE *fhandle = NULL; 								// Initialize all pointers to null :)
	if ((fhandle = fopen("smsa_data.dat", "wb")) == NULL) {				// Open file for write
		return -1;
	}
	unsigned char temp[SMSA_BLOCK_SIZE]; 						// will use to load up files
	int drm = 0;
	int blk = 0;

	smsa_client_operation(get_opcode(0x2, 0, 0), NULL); 					// seek to drm 0
	smsa_client_operation(get_opcode(0x3, 0, 0), NULL); 					// seek to blk 0
	
	while (drm < 16) {
		smsa_client_operation(get_opcode(0x4, drm, blk), temp); 			// Read blk to temp
		blk++;									// increment local blk
		fwrite(temp, sizeof(temp[0]), sizeof(temp)/sizeof(temp[0]), fhandle);	// Dump blk into file
		if (blk == 256) {							// Check if stepped off drum
			drm++;								// Increment local drum
			blk = 0;							// Reset block
			if (drm > 15) {							// Check if stepped off smsa
				break;
			}
			smsa_client_operation(get_opcode(0x2, drm, blk), NULL); 		// seek to new drm
			smsa_client_operation(get_opcode(0x3, drm, blk), NULL); 		// seek to new blk in new drm	
		}
	}

	fclose( fhandle ); 								// Close the file
	
	return ( 0 );
}
