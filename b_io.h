/**************************************************************
* Class::  CSC-415-0# Spring 2024
* Name::
* Student IDs::
* GitHub-Name::
* Group-Name::
* Project:: Basic File System
*
* File:: b_io.h
*
* Description:: Interface of basic I/O Operations
*
**************************************************************/

#ifndef _B_IO_H
#define _B_IO_H
#include <fcntl.h>

#include "structs/FreeSpace.h"
#include "mfs.h"


typedef int b_io_fd;

b_io_fd b_open (char * filename, int flags);
int b_read (b_io_fd fd, char * buffer, int count);
int b_write (b_io_fd fd, char * buffer, int count);
int b_seek (b_io_fd fd, off_t offset, int whence);
int b_close (b_io_fd fd);

typedef struct LBAFinder {
	int foundLBA;
	int remain;
} LBAFinder;

int writeBuffer(int count, b_io_fd fd, char* buffer);

int readBuffer(int count, b_io_fd fd, char* buffer);

int commitBlocks(b_io_fd fd, int nBlocks, char* buf, int calPos);

LBAFinder findLBAOnDisk(b_io_fd fd, int idxLBA);
int allocateFSBlocks(b_io_fd fd);

int trimBlocks(b_io_fd fd);
int trimBlocksHelper(b_io_fd fd, int idx);



#endif

