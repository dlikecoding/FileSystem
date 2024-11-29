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

#define N_BLOCKS 100 // NOTE: This needs to be modified to calculate the average number of blocks 

typedef struct b_fcb
	{
	/** TODO add al the information you need in the file control block **/
	// int buflen;	//holds how many valid bytes are in the buffer
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
/** Handling initialization, file descriptor allocation. Validates the file path, check the 
 * file exists (creating it if O_CREAT is specified), or truncation (O_TRUNC) or appending 
 * (O_APPEND) as needed. It sets up file metadata and allocates buffer memory for file
 * operations, returning a file descriptor for other function use 
 * @return file descriptor index or -1 on failure 
 * @author Danish Nguyen
 */
b_io_fd b_open (char * filename, int flags)
	{
	if (startup == 0) b_init();  //Initialize our system
	
	b_io_fd returnFd = b_getFCB();		// get our own file descriptor
	if (returnFd < 0) return -1; 		// check for error - all used FCB's

    time_t curTime = time(NULL);

	parsepath_st parser = { NULL, -1, "" };

	// File path is not valid
	if (parsePath(filename, &parser) != 0) return -1;
	
	// If the last element to open is ".", "..", or an empty string, it's considered invalid
	// The Linux filesystem doesn't handle this case explicitly; Not sure this should be handled here or not
	if (parser.lastElement == NULL || strcmp(parser.lastElement, "..") == 0 ||\
				 strcmp(parser.lastElement, ".") == 0 ) {
		printf("Error - please provide a valid file name\n");
		return -1;
	}
	
	if (parser.retParent[parser.index].is_directory) return -1;
		
	if (parser.index == -1 && ((flags & O_CREAT) == O_CREAT)) {
		// If the file does not exist and the flags include O_CREAT, create a new file
		parser.index = makeDirOrFile(parser, 0, NULL);
		// If the new file cannot be created, return an error
		if (parser.index == -1) return -1;
	}

	if (parser.index == -1) return -1;
		
	// Store the parent directory entry (DE) index in the fcb struct
	fcbArray[returnFd].parentIdx = parser.index;

	// Store the valid DE as file info
	fcbArray[returnFd].fi = &parser.retParent[parser.index];
	if (fcbArray[returnFd].fi == NULL) return -1;

	// If O_TRUNC is set and the file contains blocks, delete all the blocks associated with the file
	if ( (flags & O_TRUNC) ) {
		int status = removeDE(parser.retParent, parser.index, 1);
		
		// Update the modification time for the file info
		fcbArray[returnFd].fi->modification_time = curTime;
		if (status == -1) return -1;
	}

	// Update the access time for the file info
	fcbArray[returnFd].fi->access_time = curTime;

	// If O_APPEND is set, set the file pointer to the end of the file
	fcbArray[returnFd].index = (flags & O_APPEND) ? fcbArray[returnFd].fi->file_size : 0;

	// Initialize flags
	fcbArray[returnFd].flags = flags;

	fcbArray[returnFd].curBlockIdx = -1; // -1: prevents block starting at index 0

	fcbArray[returnFd].nBlocks = N_BLOCKS;

	// Allocate and initialize memory based on the number of blocks
	fcbArray[returnFd].buf = (char*) calloc(sizeof(char), B_CHUNK_SIZE);
	if (fcbArray[returnFd].buf == NULL) return -1;

	fcbArray[returnFd].curLBAPos = 0;

	return returnFd;  // all set

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

/** Receive data from the caller's buffer and write it to disk
 * @return number of bytes written to disk, or -1 on error
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
    if (fcbArray[fd].fi->ext_length == 0) {
		
		if (allocateFSBlocks(fd, 1) == -1) return -1;
		printf("Start writing data to disk...\n");
	}

	int wStatus = writeBuffer(count, fd, buffer);
	return (wStatus == 0) ? count : -1; // Return the number of bytes written
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
			int foundLBA = findLBAOnDisk(fd, fcbArray[fd].curLBAPos).foundLBA;
			int readBlocks = LBAread(buffer + callerBufPos, numBlocks, foundLBA);
			if (readBlocks < numBlocks) return -1;


			int bytesRead = B_CHUNK_SIZE * numBlocks;
			
			fcbArray[fd].index += bytesRead;
			fcbArray[fd].curLBAPos += numBlocks;
			transferByte -= bytesRead;
			
			callerBufPos += bytesRead;
			continue;
		}

		// No need to read the file on disk when buffer already has the data needed
		if (fcbArray[fd].curBlockIdx != fcbArray[fd].curLBAPos) {

			int foundLBA = findLBAOnDisk(fd, fcbArray[fd].curLBAPos).foundLBA;
			if (foundLBA == -1) return -1;

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

/** Writes data from a caller's buffer to a file buffer and disk in chunks
 * Handles both direct writes to disk and buffered writes and then write to disk for 
 * partial data. 
 * @return 0 on success, -1 on failure.
 * @author Danish Nguyen
 */
int writeBuffer(int count, b_io_fd fd, char *buffer) {
	// Tracks the position in the caller's buffer for writing progress
	int callerBufPos = 0;
	
	// Loop until all bytes in the caller's buffer are written
	while (count > 0) {

		// Calculate the current position in the file's buffer and space remaining in it
		int bufferPos = fcbArray[fd].index % B_CHUNK_SIZE; // cursor in fcb buffer
		int remainFCBBuffer = B_CHUNK_SIZE - bufferPos; // Remaining bytes in the current fcb buffer
		
		// Caller's buffer exceeds a full chunk, and file buffer is empty
		if ((count > B_CHUNK_SIZE) && (remainFCBBuffer == B_CHUNK_SIZE)) {

			// Calculate how many full blocks of data to write directly to disk
			int numBlocks = (count / B_CHUNK_SIZE);

			// Write full blocks directly to disk from the caller's buffer
			// Checks if sufficient blocks are available and writes sequentially
			if (commitBlocks(fd, numBlocks, buffer, callerBufPos) == -1) {
				printf("Error commit \n");	
				return -1;
			}
			// Update file size and remaining bytes to process
			int byteWritten = (B_CHUNK_SIZE * numBlocks);
			fcbArray[fd].index += byteWritten;
			count -= byteWritten;
			
			// Move the position in the caller's buffer forward by the bytes written
			callerBufPos += byteWritten;
			continue;
		}

		// If count is greater than the data left in the current block, transfer the 
		// remaining data in the current block. Update the index, LBAPos, and the caller's buffer position. 
		if (count > remainFCBBuffer) {

			memcpy(fcbArray[fd].buf + bufferPos, buffer + callerBufPos, remainFCBBuffer);
			
			// Write the current buffer block to disk and reset the buffer.
			if (commitBlocks(fd, 1, NULL, 0) == -1) {
				printf("Error commit \n");	
				return -1;
			}
			// Update file size and remaining bytes to process
			fcbArray[fd].index += remainFCBBuffer;
			count -= remainFCBBuffer;

			callerBufPos += remainFCBBuffer;
			continue;
		}

		// Count is smaller than the remaining data
		memcpy(fcbArray[fd].buf + bufferPos, buffer + callerBufPos, count);
		
		fcbArray[fd].index += count;
		count = 0;
		// No need to update buffer or LBA position as loop ends here
	}
	
	return 0;
}

/** Save data from a block or multiple blocks from caller's buffer to disk
 * @return 0 on success; -1 on failure
 * @author Danish Nguyen
 */
int commitBlocks(b_io_fd fd, int nBlocks, char* buffer, int callerBufPos){
	// Check if the file has enough space before start writting data.
	// If there's not enough space, try to allocate more disk blocks for the file
	if ( (fcbArray[fd].curLBAPos + nBlocks) > (fcbArray[fd].totalBlocks - 1)) {
		if ( allocateFSBlocks(fd, 2) == -1 ) return -1;
	} 
	
	// Update file size on commit
	fcbArray[fd].fi->file_size = fcbArray[fd].index;
	
	// Writing multiple blocks of data into disk. Handled discontiounus
	if (nBlocks > 1) {
		/** Iterates through the file's extents (continuous sections of disk blocks)
		 * - Uses FindLBAOnDisk to locate the starting position based on the current LBA.
		 * - Determines the smaller of the remaining blocks in the extent or the number of blocks to write.
		 * - Writes the determined number of blocks to disk and updates the caller's buffer position.
		 * - Adjusts the file descriptor's current LBA position accordingly
		 * 	Note: This approach has O(n^2) complexity and is inefficient for large files 
		 */ 

		while (nBlocks > 0) {
			LBAFinder finderLBA = findLBAOnDisk(fd, fcbArray[fd].curLBAPos);
			
			if (finderLBA.foundLBA == -1) {
				printf("Error - findLBAOnDisk: [ %d | %d] \n", fd, fcbArray[fd].curLBAPos);
				return -1;
			}
			
			int numOfBlocks =  min(finderLBA.remain, nBlocks);
			int writtenBlocks = LBAwrite(buffer + callerBufPos, numOfBlocks, finderLBA.foundLBA);
			
			if (writtenBlocks != numOfBlocks) {
				printf("Error - writtenBlocks \n");
				return -1;
			}

			callerBufPos += (B_CHUNK_SIZE * numOfBlocks);
			nBlocks -= numOfBlocks;

			// Update current pos in file to account for the blocks being written
			fcbArray[fd].curLBAPos += numOfBlocks;
		}
		return 0;
	}

	// Find starting position on the disk for the write
	// NOTE: findLBA causes an error if write is called multiple times on large file
	int idxLBA = findLBAOnDisk(fd, fcbArray[fd].curLBAPos).foundLBA;
	if (idxLBA == -1) {
		printf("Error - findLBAOnDisk: [ %d | %d] \n", fd, fcbArray[fd].curLBAPos);
		return -1;
	}

	// Update current pos in file to account for the blocks being written
	fcbArray[fd].curLBAPos += nBlocks;

	// Write buffer to disk
	int writeBlocks = LBAwrite(fcbArray[fd].buf, 1, idxLBA);

	if (writeBlocks < 1) {
		printf("Error - writtenBlocks \n");
		return -1;}

	memset(fcbArray[fd].buf, 0, B_CHUNK_SIZE);
	return 0;
}

/** Allocate more blocks on the disk for a file
 * @return 0 on success, -1 if allocation fails
 * @author Danish Nguyen
 */
int allocateFSBlocks(b_io_fd fd, int n){

	// Double the number of blocks to allocate compared to the last request
	fcbArray[fd].nBlocks *= n;

	// Request allocation of free blocks from the disk
	extents_st fileExt = allocateBlocks(fcbArray[fd].nBlocks, 0);
	if (!fileExt.size || !fileExt.extents) {
		printf("Not enough space on disk\n");
		return -1;
	}

	// Update the total block count for the file
	fcbArray[fd].totalBlocks += fcbArray[fd].nBlocks;
	
	// If the file is newly created, add the new extent to the current file
	if (fcbArray[fd].fi->ext_length == 0) {
		memcpy(fcbArray[fd].fi->extents, fileExt.extents, fileExt.size * sizeof(extent_st));
        fcbArray[fd].fi->ext_length = fileExt.size;
		
		freeExtents(&fileExt);
		return 0;
	}

	// Merge the newly allocated blocks with the file's existing extents (if possible)
	// Get the last extent of the file to check if the new blocks can be merged	
	int lastIdx = fcbArray[fd].fi->ext_length - 1;
	
	int lastStart = fcbArray[fd].fi->extents[lastIdx].startLoc;
	int lastCount = fcbArray[fd].fi->extents[lastIdx].countBlock;
	
	int index = 1; // Counter for the number of new extents added

	// Iterate through the newly allocated extents
	for (size_t i = 0; i < fileExt.size; i++) {
		int start = fileExt.extents[i].startLoc;
		int count = fileExt.extents[i].countBlock;

		// If the new extent directly follows the last extent, merge them
		if (lastStart + lastCount == start) {
			fcbArray[fd].fi->extents[lastIdx].countBlock += count;
			continue; // Skip to the next extent as it has been merged
		}

		// Otherwise, add the new extent as a separate entry
		fcbArray[fd].fi->extents[lastIdx + index].startLoc = start;
		fcbArray[fd].fi->extents[lastIdx + index].countBlock = count;
		index++;
	}

	// Adjust the length of extents in the file info
	fcbArray[fd].fi->ext_length += (index - 1);

	freeExtents(&fileExt);
	return 0;
}

/**
 * Release unused blocks back to FS map after finishing writing to the file
 * Any extents that exceed required blocks will be removed
 * @return 0 on success, -1 on failure
 * @author Danish Nguyen
 */
int trimBlocks(b_io_fd fd) {

	printf("\nFinalizing...\n");

	// Calculate the number of blocks actually needed for file size.
	int blocksUsed = computeBlockNeeded(fcbArray[fd].fi->file_size, B_CHUNK_SIZE);

	// If the number of blocks used matches the total allocated blocks, no trimming is needed
	if (blocksUsed == fcbArray[fd].totalBlocks) return 0;

	for (size_t i = 0; i < fcbArray[fd].fi->ext_length; i++) {

		// Current extent has more blocks than needed
		if (fcbArray[fd].fi->extents[i].countBlock > blocksUsed) {

			// Calculate starting location and size of the extra blocks
			int freeS = fcbArray[fd].fi->extents[i].startLoc + blocksUsed; // Start of the unused blocks
			int freeC = fcbArray[fd].fi->extents[i].countBlock - blocksUsed; // Number of unused blocks

			// Release unused blocks back to the FS map
			if (releaseBlocks(freeS, freeC) == -1) return -1;

			// Update the block count for this extent to exclude the released blocks
			fcbArray[fd].fi->extents[i].countBlock -= freeC;

			// Release any remaining unused extents after this point.
			return trimBlocksHelper(fd, i);
		}

		// If current extent matches exactly the remaining blocks needed:
		else if (fcbArray[fd].fi->extents[i].countBlock == blocksUsed) {
			// Release any remaining extents after this point.
			return trimBlocksHelper(fd, i);
		}

		// If current extent uses fewer blocks than needed:
		else {
			// Reduce number of blocks need to account for
			blocksUsed -= fcbArray[fd].fi->extents[i].countBlock;
		}
	}
	return 0;
}


/** Release and remove remaining extents from extent array after a given index
 * And then frees associated blocks back to the free space map
 * @return 0 on success; -1 on failure
 * @author Danish Nguyen
*/
int trimBlocksHelper(b_io_fd fd, int index){
	// If there are no extents left to process after the given index, return success
	if ((index + 1) >= fcbArray[fd].fi->ext_length ) return 0;

	for (size_t i = (index + 1); i < fcbArray[fd].fi->ext_length; i++) {
		int start = fcbArray[fd].fi->extents[i].startLoc;
		int count = fcbArray[fd].fi->extents[i].countBlock;
		
		if (releaseBlocks(start, count) == -1) return -1;
		fcbArray[fd].fi->ext_length --;
	}
	return 0;
}

/** Find the actual LBA on disk base on the index position
 * @return LBAFinder structure containing the actual LBA index and the number of 
 * contiguous blocks on success, or -1 if an error occurs.
 * @author Danish Nguyen
 */
LBAFinder findLBAOnDisk(b_io_fd fd, int idxLBA) {
	
	for (int i = 0; i < fcbArray[fd].fi->ext_length; i++) {
		int start = fcbArray[fd].fi->extents[i].startLoc;
		int count = fcbArray[fd].fi->extents[i].countBlock;

        if (count > idxLBA) {
			int foundLBA = start + idxLBA;
			int remain = count - (foundLBA - start);

			return (LBAFinder) { foundLBA, remain };
        } else {
            idxLBA -= count;
        }
    }
    return (LBAFinder) {-1, 0};
}







































































































// Interface to Close the file	
int b_close (b_io_fd fd)
	{	
	// printf("index: %d | Size: %d\n", fcbArray[fd].index, fcbArray[fd].fi->file_size);
	if ( (fcbArray[fd].flags & O_WRONLY) == O_WRONLY ) {
		if (fcbArray[fd].index > fcbArray[fd].fi->file_size) {
			if (commitBlocks(fd, 1, NULL, 0) == -1) return -1;
		}
		if (trimBlocks(fd) == -1) return -1;
		if (writeDirHelper(fcbArray[fd].fi - fcbArray[fd].parentIdx) == -1) return -1;
			
	}
	freePtr((void**) &fcbArray[fd].buf, "FCB buffer");

	return 0;
	}

