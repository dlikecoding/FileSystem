/**************************************************************
* Class::  CSC-415-03 FALL 2024
* Name:: Danish Nguyen, Atharva Walawalkar, Arvin Ghanizadeh, Cheryl Fong
* Student IDs:: 923091933, 924254653, 922810925, 918157791
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



#define MAXFCBS 20
#define B_CHUNK_SIZE 512

#define N_BLOCKS 100

typedef struct b_fcb
	{
	/** TODO add al the information you need in the file control block **/
	int buflen;		//holds how many valid bytes are in the buffer
	int index;		//holds the current position in the buffer

	int curLBAPos; 	// Tracks the current LBA position in file
	int curBlockIdx; // Tracks the current Block have read to bufferData 
	int totalBlocks; // Total blocks allocated on disk
	int nBlocks;  // Number of blocks allocate on disk - need to update on average file size
	int flags;		

	int parentIdx; // parent DE's index

    char* buf;		//holds the open file buffer

    directory_entry* fi; // holds infor of DE (file) 

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

	// if ((flags & O_CREAT) == O_CREAT) {
	// 	printf(" ----- Create a new file YAY \n");
	// } 

	// if ((flags & O_TRUNC) == O_TRUNC) {
	// 	printf(" ----- Remove a file \n");
	// }

	// if ((flags & O_APPEND) == O_APPEND) {
	// 	printf(" ----- Append a file \n");
	// }
	/////////////////////////////////////////////////////////////////
    if (parser.index == -1 && ( (flags & O_CREAT) == O_CREAT ) ) {
        // If the file is not exist and flas is O_CREAT, create a new files            
		parser.index = makeDirOrFile(parser, 0, NULL);
		// Can not create new file, return error
		if (parser.index == -1) return -1;
    }
	
	if (parser.index == -1) return -1;
	
	// Store parent DE's index in fcb struct
	fcbArray[returnFd].parentIdx = parser.index;

	// Store valid DE as file info
	fcbArray[returnFd].fi = &parser.retParent[parser.index];
	if (fcbArray[returnFd].fi == NULL) return -1;

    // if O_TRUNC, and the file holds blocks, delete all the blocks associate with that file
    if ( (flags & O_TRUNC) ) {
		int status = removeDE(parser.retParent, parser.index, 1);
		if (status == -1) return -1;
    }

	// if O_APPEND: Set file pointer to end if file
	fcbArray[returnFd].index = (flags & O_APPEND) ? fcbArray[returnFd].fi->file_size : 0;
	
	// Initialize ...
	fcbArray[returnFd].flags = flags;

    fcbArray[returnFd].curBlockIdx = -1; // -1: avoid block start at 0

	fcbArray[returnFd].nBlocks = N_BLOCKS;
	fcbArray[returnFd].buflen = fcbArray[returnFd].nBlocks * B_CHUNK_SIZE;

	// Allocate and initial memory base on number of blocks
	fcbArray[returnFd].buf = (char*) calloc (sizeof(char), B_CHUNK_SIZE);
	if (fcbArray[returnFd].buf == NULL) return -1;

	fcbArray[returnFd].curLBAPos = 0;
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

/** receive data from caller's buffer, and write it to disk
 * @return number of bytes had written into disk; -1 on error 
 * @author Danish Nguyen
 */
int b_write (b_io_fd fd, char * buffer, int count)
	{
	if (startup == 0) b_init();  //Initialize our system

	// check that fd is between 0 and (MAXFCBS-1)
	if ((fd < 0) || (fd >= MAXFCBS) || fcbArray[fd].fi == NULL )
		{
		return (-1); 					//invalid file descriptor
		}
	
	// File is opened in write-only mode
    if ((fcbArray[fd].flags & O_WRONLY) != O_WRONLY) return -1;

    // Allocate free space on disk and make sure the disk has enough space.
    if (!fcbArray[fd].fi->file_size) {
		extents_st fileExt = allocateBlocks(fcbArray[fd].nBlocks, 0);

		if (!fileExt.size || !fileExt.extents) {
			printf("Not enough space on disk\n");
			return -1;
		}

		memcpy(fcbArray[fd].fi->extents, fileExt.extents, fileExt.size * sizeof(extent_st));
        fcbArray[fd].fi->ext_length = fileExt.size;
		
		freeExtents(&fileExt);
		fcbArray[fd].totalBlocks += fcbArray[fd].nBlocks;
		printf("Start writing data to disk...\n");
	}

	int writeStatus = writeBuffer(count, fd, buffer);
	return (writeStatus == 0) ? count : -1; // Return the number of bytes written
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

	if ( (fcbArray[fd].flags & O_RDONLY) != O_RDONLY ) return -1;

	// Track the number of bytes that have not been read from the file
	int readBytes = fcbArray[fd].fi->file_size - fcbArray[fd].index; 

	// Track bytes to transfer based on available data in the file
	int transferred = (readBytes > count) ? count : readBytes;
	
	int status = readBuffer(transferred, fd, buffer);

	
	return ( status == 0 ) ? transferred : status;
	}
	

int writeBuffer(int count, b_io_fd fd, char *buffer) {
	
	// An offset for tracking # of bytes has been written from caller's buffer
	int callerBufPos = 0;

	while (count > 0) {
		int bufferPos = fcbArray[fd].fi->file_size % B_CHUNK_SIZE; // cursor in fcb buffer
		int remainFCBBuffer = B_CHUNK_SIZE - bufferPos; // Remaining bytes in the current fcb buffer
		
		// If count exceeds chunk size and a full block of data is available to write
		if ((count > B_CHUNK_SIZE) && (remainFCBBuffer == B_CHUNK_SIZE)) {
			// Calculate how many full blocks need to be written to disk
			int numBlocks = (count / B_CHUNK_SIZE);

			/** Check if there are enough free blocks before committing
			 * Commit the number of blocks from the caller's buffer to disk
			 * Check if one extent has enough blocks; if so, LBA-write one extent,
			 * otherwise, loop and write.
			 */
			if (commitBlocks(fd, numBlocks, buffer, callerBufPos) == -1) return -1;

			int byteWritten = (B_CHUNK_SIZE * numBlocks);
			fcbArray[fd].fi->file_size += byteWritten;
			count -= byteWritten;
			
			callerBufPos += byteWritten;
			continue;
		}

		// If count is greater than the data left in the current block, transfer the 
		// remaining data in the current block. Update the index, LBAPos, and the caller's buffer position. 
		if (count > remainFCBBuffer) {
			memcpy(fcbArray[fd].buf + bufferPos, buffer + callerBufPos, remainFCBBuffer);
			
			/** Check if there are enough free blocks before committing
			 * Commit one block from the FCB's buffer to disk. LBAwrite one block
			 */
			if (commitBlocks(fd, 1, NULL, 0) == -1) return -1;

			fcbArray[fd].fi->file_size += remainFCBBuffer;
			count -= remainFCBBuffer;

			callerBufPos += remainFCBBuffer;
			continue;
		}
		
		// count is smaller than the remaining data
		memcpy(fcbArray[fd].buf + bufferPos, buffer + callerBufPos, count);

		fcbArray[fd].fi->file_size += count;
		count = 0;
		// No need to update buffer or LBA position as loop ends here
	}
	return 0;
}

// commitBlocks -> write nBlock to disk, clear buffer
int commitBlocks(b_io_fd fd, int nBlocks, char* buffer, int callerBufPos){
	// Check number of blocks needed to write data to disk not enough, allocate more block
	if ( (fcbArray[fd].curLBAPos + nBlocks) > (fcbArray[fd].totalBlocks - 1)) {
		if ( allocateFSBlocks(fd) == -1 ) return -1;
	} 

	// Find actual LBA index on disk
	int foundIdx = findLBAOnDisk(fd, fcbArray[fd].curLBAPos);
	if (foundIdx == -1) return -1;


	fcbArray[fd].curLBAPos += nBlocks; // Update number of blocks written to disk
	
	// Write multiple blocks at the same
	if (nBlocks > 1) {
		// Need to check if the count of an extent is greater than the block size
		////////////////////////////////////////////////////////////////

		int writeBlocks = LBAwrite(buffer + callerBufPos, nBlocks, foundIdx);
		if (writeBlocks < nBlocks) return -1;
		
		printf(".");
		return 0;
	}

	// Write buffer to disk
	int readBlocks = LBAwrite(fcbArray[fd].buf, 1, foundIdx);
	if (readBlocks < 1) return -1;

	memset(fcbArray[fd].buf, 0, B_CHUNK_SIZE);
	printf("#");
	return 0;
}

// allocate more block on disk to write more data in to it.
int allocateFSBlocks(b_io_fd fd){

	// Update nBlock value for the next allocate block will double size number of blocks
	fcbArray[fd].nBlocks *= 2; // Double size number of blocks request
	fcbArray[fd].totalBlocks += fcbArray[fd].nBlocks;

	extents_st fileExt = allocateBlocks(fcbArray[fd].nBlocks, 0);
	if (!fileExt.size || !fileExt.extents) {
		printf("Not enough space on disk\n");
		return -1;
	}
	
	// Add allocated extents to the current extent in file info
	// Can only merge at the last index of array's extent for files
	int lastIdx = fcbArray[fd].fi->ext_length - 1;
	
	int lastStart = fcbArray[fd].fi->extents[lastIdx].startLoc;
	int lastCount = fcbArray[fd].fi->extents[lastIdx].countBlock;
		
	int index = 1;
	for (size_t i = 0; i < fileExt.size; i++) {
		int start = fileExt.extents[i].startLoc;
		int count = fileExt.extents[i].countBlock;

		// Merge last extent in file if possible
		if (lastStart + lastCount == start) {
			fcbArray[fd].fi->extents[lastIdx].countBlock += count;
			continue; // Need to check on what index need to skip
		}

		// otherwise, set the next extent equals to the new extents allocated
		fcbArray[fd].fi->extents[lastIdx + index].startLoc = start;
		fcbArray[fd].fi->extents[lastIdx + index].countBlock = count;
		index++;
	}

	// Increase the size of extents base on how many extent added
	fcbArray[fd].fi->ext_length += (index - 1);

	freeExtents(&fileExt);
	return 0;
}

/** Release the remaining blocks to freespace map after finished writing.
 * @return 0 on success; -1 on failure
 */
int trimBlocks(b_io_fd fd) {
	// | 31 | 2 | 	5	| 20 | 6 |   
	// | 14 | 2 | 	3
	// | 26 | 1 | 	1
	// | 19 | 4 |
	printf("\nFinalizing...\n");
	int blocksUsed = computeBlockNeeded(fcbArray[fd].fi->file_size, B_CHUNK_SIZE);
	if (blocksUsed == fcbArray[fd].totalBlocks) return 0;
	
	for (size_t i = 0; i < fcbArray[fd].fi->ext_length; i++) {

		if (fcbArray[fd].fi->extents[i].countBlock > blocksUsed) {

			int freeS = fcbArray[fd].fi->extents[i].startLoc + blocksUsed;
			int freeC = fcbArray[fd].fi->extents[i].countBlock - blocksUsed;
			// Release one extent has more blocks that it should
			if (releaseBlocks(freeS, freeC) == -1) return -1;
			
			// Update count block in file info
			fcbArray[fd].fi->extents[i].countBlock -= freeC;
			
			//Release all remaining unuse extents to freespace
			return trimBlocksHelper(fd, i);

		} else if (fcbArray[fd].fi->extents[i].countBlock == blocksUsed) {
			return trimBlocksHelper(fd, i);

		} else {
			blocksUsed =- fcbArray[fd].fi->extents[i].countBlock;
		}
	}

	//////////////////////////////////////////////////////////////////
	// for (size_t i = 0; i < fcbArray[fd].fi->ext_length; i++) {
	// 	printf("\n ------- [ %d | %d ] -------\n", 
	// 	fcbArray[fd].fi->extents[i].startLoc, fcbArray[fd].fi->extents[i].countBlock );
	// }

	// printf("\n ----- blocksUsed: %d | nblock: %d | totalblock: %d\n", 
	// 	blocksUsed, fcbArray[fd].nBlocks, fcbArray[fd].totalBlocks );
	///////////////////////////////////////////////////////////////////
	printf("Data write complete!\n");
	return 0;
}

/** Release unused helper: release and remove remaining extent in extent's array 
 * @return 0 on success; -1 on failure
*/
int trimBlocksHelper(b_io_fd fd, int index){
	for (size_t j = (index + 1); j < fcbArray[fd].fi->ext_length; j++) {
		int start = fcbArray[fd].fi->extents[j].startLoc;
		int count = fcbArray[fd].fi->extents[j].countBlock;
		
		if (releaseBlocks(start, count) == -1) return -1;
		fcbArray[fd].fi->ext_length --;
	}
	return 0;
}

/** Find the actual LBA on disk base on the index
 * @return int actual LBA index on success, -1 on error
 * @author Danish Nguyen
 */
int findLBAOnDisk(b_io_fd fd, int idxLBA) {
    for (int i = 0; i < fcbArray[fd].fi->ext_length; ++i) {
    
        if (fcbArray[fd].fi->extents[i].countBlock > idxLBA) {
            return fcbArray[fd].fi->extents[i].startLoc + idxLBA; // actual LBA on disk
        
        } else {
            idxLBA -= fcbArray[fd].fi->extents[i].countBlock;
        }
    }
    return -1;
}


// Interface to Close the file	
int b_close (b_io_fd fd)
	{

	if ( (fcbArray[fd].flags & O_WRONLY) == O_WRONLY ) {
		if (strlen(fcbArray[fd].buf) > 0) {
			if (commitBlocks(fd, 1, NULL, 0) == -1) return -1;
			if (trimBlocks(fd) == -1) return -1;
		}
		if (writeDirHelper(fcbArray[fd].fi - fcbArray[fd].parentIdx) == -1) return -1;
			
	}
	
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
			int numBlocks = (transferByte / B_CHUNK_SIZE);

			// Transfer number of blocks into caller's buffer, starting from current LBA position
			int foundLBA = findLBAOnDisk(fd, fcbArray[fd].curLBAPos);
			int readBlocks = LBAread(buffer + callerBufPos, numBlocks, foundLBA);
			if (readBlocks < numBlocks) return -1;

			fcbArray[fd].index += (B_CHUNK_SIZE * numBlocks);
			fcbArray[fd].curLBAPos += numBlocks;
			transferByte -= (B_CHUNK_SIZE * numBlocks);
			
			callerBufPos += (B_CHUNK_SIZE * numBlocks);
			continue;
		}

		// No need to read the file on disk when buffer already has the data needed
		if (fcbArray[fd].curBlockIdx != fcbArray[fd].curLBAPos) {

			int foundLBA = findLBAOnDisk(fd, fcbArray[fd].curLBAPos);

			int readBlocks = LBAread(fcbArray[fd].buf, 1, foundLBA);
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
