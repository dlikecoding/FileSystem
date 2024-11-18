/**************************************************************
* Class::  CSC-415-03 FALL 2024
* Name:: Danish Nguyen
* Student IDs:: 923091933
* GitHub-Name:: dlikecoding
* Group-Name:: 0xAACD
* Project:: Basic File System
*
* File:: b_io.c
*
* Description:: Basic File System - Key File I/O Operations
*
**************************************************************/

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>			// for malloc
#include <string.h>			// for memcpy
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include "b_io.h"

#include "mfs.h"

#define MAXFCBS 20
#define B_CHUNK_SIZE 512

#define N_BLOCKS 2

typedef struct b_fcb
	{
	/** TODO add al the information you need in the file control block **/
	int buflen;		//holds how many valid bytes are in the buffer
	int index;		//holds the current position in the buffer

	int curLBAPos; 	// Tracks the current LBA position in file
	int curBlockIdx; // Tracks the current Block have read to bufferData 
	
	int countCpy;	// Total bytes copy to compare with filesize
	int numOfReqs;  // Number of blocks allocate on disk - need to update on average file size
	extents_st wExt;// holds extents of blocks locations 

	int flags;

    int parentIdx;  // parent index

    directory_entry* fi; // parent DE
    char* buf;		//holds the open file buffer


	} b_fcb;
	
b_fcb fcbArray[MAXFCBS];

int startup = 0;	//Indicates that this has not been initialized

//Method to initialize our file system
void b_init ()
	{
	//init fcbArray to all free
	for (int i = 0; i < MAXFCBS; i++)
		{
		fcbArray[i].buf = NULL; //indicates a free fcbArray
		}
		
	startup = 1;
	}

//Method to get a free FCB element
b_io_fd b_getFCB ()
	{
	for (int i = 0; i < MAXFCBS; i++)
		{
		if (fcbArray[i].buf == NULL)
			{
			return i;		//Not thread safe (But do not worry about it for this assignment)
			}
		}
	return (-1);  //all in use
	}
	
// Interface to open a buffered file
// Modification of interface for this assignment, flags match the Linux flags 
// for open O_RDONLY, O_WRONLY, or O_RDWR
b_io_fd b_open (char * filename, int flags)
	{
	//*** TODO ***:  Modify to save or set any information needed
	if (startup == 0) b_init();  //Initialize our system
	
	b_io_fd returnFd = b_getFCB();		// get our own file descriptor
	if (returnFd < 0) return -1; 		// check for error - all used FCB's

	parsepath_st parser = { NULL, -1, "" };

	// file path is not valid, or is a directory
	if (parsePath(filename, &parser) != 0 || parser.retParent[parser.index].is_directory) return -1;
	
	/////////////////////////////////////////////////////////////////
	// O_CREAT - Creates a new file is it does not exist 
	// (ignored if the file does exist)
	// O_TRUNC - Set the file length to 0 (truncates all data)
	// O_APPEND - Sets the file position to the end of the file (Same as doing a seek 0 from SEEK_END)
	// O_RDONLY - File can only do read/seek operations
	// O_WRONLY - File can only do write/seek operations
	// O_RDWR - File can be read or written to

	if ((flags & O_CREAT) == O_CREAT) {
		printf(" ----- Create a new file YAY \n");
	} 

	if ((flags & O_TRUNC) == O_TRUNC) {
		printf(" ----- Remove a file \n");
	}

	if ((flags & O_APPEND) == O_APPEND) {
		printf(" ----- Append a file \n");
	}
	/////////////////////////////////////////////////////////////////
    if (parser.index == -1 && ( (flags & O_CREAT) == O_CREAT ) ) {
        // If the file is not exist and flas is O_CREAT, create a new files            
		parser.index = makeDirOrFile(parser, 0, NULL);
		// Can not create new file, return error
		if (parser.index == -1) return -1;
    }

	if (parser.index == -1) return -1;

	// Store valid DE into file info
	fcbArray[returnFd].fi = &parser.retParent[parser.index];
	if (fcbArray[returnFd].fi == NULL) return -1;

    // if O_TRUNC, delete all the blocks associate with that file
    if (flags & O_TRUNC) {
		int status = removeDE(fcbArray[returnFd].fi, parser.index, 1);
		if (status == -1) return -1;
    }

	// if O_APPEND: Set file pointer to end if file
	fcbArray[returnFd].index = (flags & O_APPEND) ? fcbArray[returnFd].fi->file_size : 0;
	
	// Initialize ...
	fcbArray[returnFd].flags = flags;

    fcbArray[returnFd].buf = NULL; // Allocate a block of buffer to store data read from the disk
	fcbArray[returnFd].buflen = 0; 
	fcbArray[returnFd].curBlockIdx = -1; // -1: avoid block start at 0

	fcbArray[returnFd].numOfReqs = N_BLOCKS;
	fcbArray[returnFd].countCpy = 0;

	fcbArray[returnFd].wExt = (extents_st) {NULL, 0};

	// fcbArray[returnFd].curLBAPos = fcbArray[returnFd].fi->extents->startLoc;
	return (returnFd);		// all set
	}


// Interface to seek function	
int b_seek (b_io_fd fd, off_t offset, int whence)
	{
	if (startup == 0) b_init();  //Initialize our system

	// check that fd is between 0 and (MAXFCBS-1)
	if ((fd < 0) || (fd >= MAXFCBS))
		{
		return (-1); 					//invalid file descriptor
		}
		
		
	return (0); //Change this
	}



// Interface to write function	
int b_write (b_io_fd fd, char * buffer, int count)
	{
	if (startup == 0) b_init();  //Initialize our system

	// check that fd is between 0 and (MAXFCBS-1)
	if ((fd < 0) || (fd >= MAXFCBS) || fcbArray[fd].fi == NULL )
		{
		return (-1); 					//invalid file descriptor
		}
	///////////////////////////////////////////////////////////////
    // File is opened in write-only mode
    if ((fcbArray[fd].flags & O_WRONLY) != O_WRONLY) {
        return -1;
    }
    // printf( "====== buf: %s - count %d - fd: %d======\n", buffer, count, fd );
    // printf( "%s", buffer);
    
    
    // Allocate free space on disk to make sure the disk has enough space.
    if (!fcbArray[fd].wExt.size) {
		fcbArray[fd].wExt = allocateBlocks(fcbArray[fd].numOfReqs, 0);

		if (!fcbArray[fd].wExt.size) {
			printf("Not enough space on disk\n");
			return -1;
		}
		printf(" Allocated: [%d: %d]\n", fcbArray[fd].wExt.extents->startLoc, fcbArray[fd].wExt.extents->countBlock);		
	}
    
    // Allocate and initial memory base on number of blocks
	if (!fcbArray[fd].buf) {
		fcbArray[fd].buf = (char*) calloc( fcbArray[fd].numOfReqs, B_CHUNK_SIZE);
		if (fcbArray[fd].buf == NULL) return -1;
		fcbArray[fd].buflen = fcbArray[fd].numOfReqs * B_CHUNK_SIZE;
	}
	
	printf("%s",buffer);
	
	int cpyToBuf = count;
	while (cpyToBuf > 0) { 

        int remainInBuf = fcbArray[fd].buflen - fcbArray[fd].index;

        if (cpyToBuf > remainInBuf) {
            printf("cpyToBuf > remainInBuf\ncpyToBuf: %d - remainInBuf: %d\n", cpyToBuf, remainInBuf);
			memcpy(fcbArray[fd].buf + fcbArray[fd].index, buffer, remainInBuf);
            cpyToBuf -= remainInBuf;
			
			int cStatus = commitBlocks(fd);
			int aStatus = adjustBufferSize(fd);
			if (cStatus == -1 || aStatus == -1) return -1;
			printf("commited\ncpyToBuf: %d - remainInBuf: %d\n", cpyToBuf, remainInBuf);
			
			memcpy(fcbArray[fd].buf, buffer + cpyToBuf, cpyToBuf);
			cpyToBuf -= cpyToBuf;
			fcbArray[fd].index += cpyToBuf;

        } else {
			printf("else\ncpyToBuf: %d - remainInBuf: %d\n", cpyToBuf, remainInBuf);
			
			memcpy(fcbArray[fd].buf + fcbArray[fd].index, buffer, cpyToBuf);
			
			cpyToBuf -= cpyToBuf;
			fcbArray[fd].index += cpyToBuf;
		}
		
	}
		
		
    // // }

	// int remainInBuf = fcbArray[fd].buflen - fcbArray[fd].index; 

	// // Track bytes to transfer based on available data in the file
	// int writeToBuf = (remainInBuf > count) ? count : remainInBuf;
	// // printf("count: %d - remainInBuf: %d\n", count, remainInBuf);
	// // int status = writeBuffer(writeToBuf, remainInBuf, fd, buffer);

	// if (remainInBuf < count){
	// 	memcpy(fcbArray[fd].buf + fcbArray[fd].index, buffer, remainInBuf);
	// 	int cStatus = commitBlocks(fd);
	// 	int aStatus = adjustBufferSize(fd);

	// 	int copy = count - remainInBuf;
	// 	memcpy(fcbArray[fd].buf, buffer + copy, copy);
		
		
	// } else {
	// 	memcpy(fcbArray[fd].buf + fcbArray[fd].index, buffer, count); 
		
	// }
	// fcbArray[fd].index += count;

	writeBuffer(count, fd, buffer);
	return count; // Return the number of bytes written
	}

int writeBuffer(int count, b_io_fd fd, char* buffer) {
	// printf("%s",buffer);
	
	// int cpyToBuf = count;
	// while (cpyToBuf > 0) { 

    //     int remainInBuf = fcbArray[fd].buflen - fcbArray[fd].index;

    //     if (cpyToBuf > remainInBuf) {
    //         printf("cpyToBuf > remainInBuf\ncpyToBuf: %d - remainInBuf: %d\n", cpyToBuf, remainInBuf);
	// 		memcpy(fcbArray[fd].buf + fcbArray[fd].index, buffer, remainInBuf);
    //         cpyToBuf -= remainInBuf;
			
	// 		int cStatus = commitBlocks(fd);
	// 		int aStatus = adjustBufferSize(fd);
	// 		if (cStatus == -1 || aStatus == -1) return -1;
	// 		printf("commited\ncpyToBuf: %d - remainInBuf: %d\n", cpyToBuf, remainInBuf);
			
	// 		memcpy(fcbArray[fd].buf, buffer + cpyToBuf, cpyToBuf);
	// 		cpyToBuf -= cpyToBuf;
	// 		fcbArray[fd].index += cpyToBuf;

    //     } else {
	// 		printf("else\ncpyToBuf: %d - remainInBuf: %d\n", cpyToBuf, remainInBuf);
			
	// 		memcpy(fcbArray[fd].buf + fcbArray[fd].index, buffer, cpyToBuf);
			
	// 		cpyToBuf -= cpyToBuf;
	// 		fcbArray[fd].index += cpyToBuf;
	// 	}
		
	// }
	// Write Completed
	return 0;
}


// Interface to read a buffer
// Filling the callers request is broken into three parts
// Part 1 is what can be filled from the current buffer, which may or may not be enough
// Part 2 is after using what was left in our buffer there is still 1 or more block
//        size chunks needed to fill the callers request.  This represents the number of
//        bytes in multiples of the blocksize.
// Part 3 is a value less than blocksize which is what remains to copy to the callers buffer
//        after fulfilling part 1 and part 2.  This would always be filled from a refill 
//        of our buffer.
//  +-------------+------------------------------------------------+--------+
//  |             |                                                |        |
//  | filled from |  filled direct in multiples of the block size  | filled |
//  | existing    |                                                | from   |
//  | buffer      |                                                |refilled|
//  |             |                                                | buffer |
//  |             |                                                |        |
//  | Part1       |  Part 2                                        | Part3  |
//  +-------------+------------------------------------------------+--------+
int b_read (b_io_fd fd, char * buffer, int count)
	{
	printf( "====== buf: %s - count %d - fd: %d======\n", buffer, count, fd );
	if ( (fcbArray[fd].flags & O_RDONLY) == O_RDONLY ) {
		printf( "****** b_read ******\n");
	}
	if (startup == 0) b_init();  //Initialize our system

	// check that fd is between 0 and (MAXFCBS-1)
	if ((fd < 0) || (fd >= MAXFCBS))
		{
		return (-1); 					//invalid file descriptor
		}
	
	if (fcbArray[fd].fi == NULL)		//File not open for this descriptor
		{
		return -1;
		}

	///////////////////////////////////////////////////////////////
	// Write only is not support in READ
	if ( (fcbArray[fd].flags & O_RDONLY) != O_RDONLY ) return -1;

	// Track the number of bytes that have not been read from the file
	int readBytes = fcbArray[fd].fi->file_size - fcbArray[fd].index; 

	// Track bytes to transfer based on available data in the file
	int transferred = (readBytes > count) ? count : readBytes;

	int status = readBuffer(transferred, fd, buffer);


	return ( status == 0 ) ? transferred : status;
	}
	
// Interface to Close the file	
int b_close (b_io_fd fd)
	{
	printf("\n------ b_close: %d ------ \n", fcbArray[fd].index);
	printf("%s", fcbArray[fd].buf);

	if ( (fcbArray[fd].flags & O_RDONLY) == O_RDONLY ) {
		if (fcbArray[fd].index > 0) commitBlocks(fd);
	}
	

	freeExtents(&fcbArray[fd].wExt);
	freePtr((void**) &fcbArray[fd].buf, "FCB buffer");
	return 0;
	}

/**
 * Transfers data from a file into the caller's buffer. It reads data in chunks and 
 * copies the requested number of bytes.
 * @param transferByte (int): number of bytes to transfer from the file to the buffer
 * @param fd (b_io_fd): file descriptor index
 * @param buffer (char*): caller's buffer
 * @return 0 success, -1 error
 * @author Danish Nguyen
 */
int readBuffer(int transferByte, b_io_fd fd, char *buffer) {
	// An offset for tracking # of bytes has been copied into caller's buffer
	int callerBufPos = 0;
	
	/* Loop until all data is transferred to the caller's buffer. Handles three cases:
		1. Large data transfer: transferByte > B_CHUNK_SIZE and full block is not read,
		copy multiple blocks at once. Then update index, curLBAPos, transferByte, and 
		caller's Buffer.
		2. Partial block transfer: bytes to transfer exceed remaining block data, copy 
		remaining data first. Then update index, curLBAPos, transferByte, and 
		caller's Buffer.
		3. Small transfer: transferByte is less than remaining block data, copy to 
		caller's buffer */

	while (transferByte > 0) {
		int readInBlock = fcbArray[fd].index % B_CHUNK_SIZE; // Data has been read in a block
		int remainInBlock = B_CHUNK_SIZE - readInBlock; // Remaining bytes in the current block
		
		// transferByte exceeds chunk size and a full block of data is avalible to read
		if ( (transferByte > B_CHUNK_SIZE) && (remainInBlock == B_CHUNK_SIZE) ) {
			// To calculates how many full blocks to read from disk
			uint64_t numBlocks = (transferByte / B_CHUNK_SIZE);

			// Transfer number of blocks into caller's buffer, starting from current LBA position
			uint64_t readBlocks = LBAread(buffer + callerBufPos, numBlocks, fcbArray[fd].curLBAPos);
			if (readBlocks < numBlocks) return -1;

			fcbArray[fd].index += (B_CHUNK_SIZE * numBlocks);
			fcbArray[fd].curLBAPos += numBlocks;
			transferByte -= (B_CHUNK_SIZE * numBlocks);
			
			callerBufPos += (B_CHUNK_SIZE * numBlocks);
			continue;
		}

		// No need to read the file on disk when buffer already has the data needed
		if (fcbArray[fd].curBlockIdx != fcbArray[fd].curLBAPos) {
			uint64_t readBlocks = LBAread(fcbArray[fd].buf, 1, fcbArray[fd].curLBAPos);
			if (readBlocks < 1) return -1;

			fcbArray[fd].curBlockIdx = fcbArray[fd].curLBAPos;
		}

		/* transferByte is greater than or equal data left in the current block, transfer the 
		remaining data in current block and update index, LBAPos and caller' buffer position */
		if (transferByte >= remainInBlock) {
			memcpy(buffer + callerBufPos, fcbArray[fd].buf + readInBlock, remainInBlock);
			
			fcbArray[fd].index += remainInBlock;
			fcbArray[fd].curLBAPos ++;
			transferByte -= remainInBlock;

			callerBufPos += remainInBlock;
			continue;
		}
		
		// transferByte is smaller than the remaining data
		memcpy(buffer + callerBufPos, fcbArray[fd].buf + readInBlock, transferByte);

		fcbArray[fd].index += transferByte;
		transferByte = 0;
		// No need to update buffer or LBA position as loop ends here
	}
	return 0;
}

// commitBlocks -> write nBlock to disk, clear buffer, update fileSize (fileSize += fd.index)
int commitBlocks(b_io_fd fd){
	printf("\n*************Committing blocks at LBA index ****************\n");
	// printf("%s", fcbArray[fd].buf);
	for (size_t i = 0; i < fcbArray[fd].wExt.size; i++) {
		// printf("Write extent [start: %d | end: %d]", fcbArray[fd].wExt.extents[i].startLoc, fcbArray[fd].wExt.extents[i].countBlock);
	}
	
	fcbArray[fd].fi->file_size += fcbArray[fd].index; //Update size
	fcbArray[fd].index = 0; // Reset fd.index
	fcbArray[fd].numOfReqs *= 2; // Double size number of blocks request
	// printf("Write extent [numOfReqs: %d | index: %d]", fcbArray[fd].numOfReqs, fcbArray[fd].index);

	return 0;
}

// resize fd.buffer and clean old buffer. 
int adjustBufferSize(b_io_fd fd){

	freeExtents(&fcbArray[fd].wExt);

	fcbArray[fd].wExt = allocateBlocks(fcbArray[fd].numOfReqs, 0);
	if (!fcbArray[fd].wExt.size) {
		printf("Not enough space on disk\n");
		return -1;
	}
	// printf(" Allocated: [%d: %d]\n", fcbArray[fd].wExt.extents->startLoc, fcbArray[fd].wExt.extents->countBlock);


	// Update nBlock value
	
	int newSize = fcbArray[fd].numOfReqs * B_CHUNK_SIZE;
	// printf(" adjustBufferSize - fcbArray[fd].numOfReqs: %d \n", newSize);
	// freePtr((void**) &fcbArray[fd].buf, "fd buff");
	fcbArray[fd].buf = realloc(fcbArray[fd].buf, newSize);
	if (!fcbArray[fd].buf) {
		printf("Error: calloc\n");
		return -1;
	}
	memset(fcbArray[fd].buf, 0, newSize);
	fcbArray[fd].buflen = newSize;
	return 0;
}


/** Find the actual LBA on disk base on the index
 * @return int actual LBA on success, -1 on error 
 */
int findLBAOnDisk(extents_st exts, int idxLBA) {
    int loc = idxLBA;

    for (int i = 0; i < exts.size; ++i) {
        
        if (exts.extents[i].countBlock > loc) {
            int actualLBA = exts.extents[i].startLoc + loc; // actual LBA on disk
            printf("Translated LBA %d to disk LBA %d\n", idxLBA, actualLBA);
            return actualLBA; 
        
        } else {
            loc -= exts.extents[i].countBlock;
        }
    }
    return -1;
}
