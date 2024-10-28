/**************************************************************
* Class::  CSC-415-03 FALL 2024
* Name:: Danish Nguyen
* Student IDs:: 923091933
* GitHub-Name:: dlikecoding
* Group-Name:: 0xAACD
* Project:: Basic File System
*
* File:: fsInit.c
*
* Description:: Main driver for file system assignment.
*
* This file is where you will start and initialize your system
*
**************************************************************/

#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <stdio.h>
#include <string.h>

#include "fsLow.h"
#include "mfs.h"

#include "structs/VCB.h"
#include "structs/FreeSpace.h"
#include "structs/DE.h"

#define SIGNATURE 6565676850526897110

volume_control_block * vcb;

int initFileSystem (uint64_t numberOfBlocks, uint64_t blockSize)
	{
	printf ("Initializing File System with %ld blocks with a block size of %ld\n", numberOfBlocks, blockSize);
	/* TODO: Add any code you need to initialize your file system. */
	
	//Allocate first block on the disk into memory which store vcb struct
	vcb = malloc(blockSize);
	if (vcb == NULL) return -1;

	//Read first block on disk & return if error
	if (LBAread(vcb, 1, 0) < 1) return -1;

	//File System is matched with current FS
	if (vcb->signature == SIGNATURE) {
		//Load free_space_loc to VCB ptr
		//Load root_loc to VCB ptr
		
		printf("FOUND: %ld \n", vcb->signature);
		printf("Init : totalFreeBs: %d, extentsLen: %d \n", vcb->fs_st.totalBlocksFree, vcb->fs_st.extentLength);
		
		// request_blocks getBlocks = allocateBlocks(300,100);
		// if (getBlocks.extents == NULL) return 0;

		// for (size_t i = 0; i < getBlocks.size; i++) {
		// 	printf(" ===[%d: %d]=== \n", getBlocks.extents[i].startLoc, getBlocks.extents[i].countBlock);
		// }
		
		// printf("AFTER : totalFreeBs: %d, extentsLen: %d \n", vcb->fs_st.totalBlocksFree, vcb->fs_st.extentLength);

		// releaseBlocks(17900, 100);
		// free(getBlocks.extents);
		// getBlocks.extents = NULL;

		return 0;
	}

	// When sig not found - Initialize a new disk and reformat
	printf("Do you want to format the disk for new FileSystem? \nAssume: Yes!\n");
	printf("Enter volume's name: Assume entered 'FS-Project'\n");
	strcpy(vcb->volume_name, "FS-Project");
	vcb->signature = SIGNATURE;
	vcb->total_blocks = numberOfBlocks;
	vcb->block_size = blockSize;

	int initFSStatus = initFreeSpace(numberOfBlocks, blockSize);
	if (initFSStatus != 0) return -1;
	
	
	// Allocatte memory for root_dir_ptr	
	vcb->root_dir_ptr = createDirectory(50, NULL);

	//DirectoryEntry location start after FreeSpace loc
	vcb->root_loc = vcb->root_dir_ptr->block_location.startLoc;
    
	//Write Volumn Control Block back to the disk
	if (LBAwrite(vcb, 1, 0) < 1) return -1; //write unsuccessed

	return 0;
	}
	
	
void exitFileSystem ()
	{
	free(vcb);
	vcb = NULL;

	printf ("System exiting\n");
	}