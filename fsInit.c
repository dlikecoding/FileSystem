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

	vcb->free_space_map = NULL; 
	vcb->root_dir_ptr = NULL;
	vcb->fs_st.terExtTBMap = NULL;

	//File System is matched with current FS
	if (vcb->signature == SIGNATURE) {
		/* TODO: Load free_space_map and root_dir_ptr location HERE */
		vcb->root_dir_ptr = loadDirectoryEntry();
		vcb->free_space_map = loadFreeSpaceMap(FREESPACE_START_LOC);
		
		if (vcb->root_dir_ptr == NULL || vcb->free_space_map == NULL ) return -1;
		// vcb->fs_st.terExtTBMap = NULL;

		printf("============== root_loc: %d =============\n", vcb->root_loc);
		printf("============== free_space_loc: %d =============\n", vcb->free_space_loc);
		

		// int i = 0;
		// while (vcb->root_dir_ptr->file_size != 0) {
		// 	printf("================%s=============", vcb->root_dir_ptr->file_name);
		// 	vcb->root_dir_ptr++;
		// }

		/* TEST ALLOCATE */
		// for (size_t i = 0; i < 1100 ; i++) {
			// extents_st test = allocateBlocks(1, 0);
		// 	if (test.extents != NULL) {
		// 		printf("============== [%d: %d] =============\n", test.extents[0].startLoc, test.extents[0].countBlock);
		// 	}
		// }

		/* TEST RELEASE */
		// for (size_t id = 30; id < 1100 ; id++) { //1024
		// 	// if (id % 2 == 0) {
		// 		int test = releaseBlocks(id, 1);
		// 		printf("============== currentA: %d (%ld, 1) =============\n", vcb->fs_st.curExtentLBA, id);
		// 	// }	
		// }
		
		// vcb->fs_st.terExtTBMap = (int*) allocateMemFS(1);

        // int readStatus = LBAread(vcb->fs_st.terExtTBMap, 1, 1073);

		// int secondTab = getSecTBLocation(0);
		// printf( "secondTab: %d", secondTab);

		vcb->free_space_map = loadFreeSpaceMap(2226);
		for (size_t i = 0; i < 100 ; i++) { //vcb->fs_st.extentLength
			printf("============== [ %d: %d ] =============\n", vcb->free_space_map[i].startLoc, vcb->free_space_map[i].countBlock);
		}
	
		printf("\n -- Total Blocks Free: %d - Extent Length: %d - terExtTBLoc: %d =============\n", 
				vcb->fs_st.totalBlocksFree, vcb->fs_st.extentLength, vcb->fs_st.terExtTBLoc);
		
		return 0;
	}

	// When sig not found - Initialize a new disk and reformat
	printf("Do you want to format the disk for new FileSystem? \nAssume: Yes!\n");
	printf("Enter volume's name: Assume entered 'FS-Project'\n");
	strcpy(vcb->volume_name, "FS-Project");
	vcb->signature = SIGNATURE;
	vcb->total_blocks = numberOfBlocks;
	vcb->block_size = blockSize;
	
	// Load the free space map into memory
	vcb->free_space_map = initFreeSpace(numberOfBlocks, blockSize);
	if (vcb->free_space_map == NULL) return -1;

	// // Allocatte memory for root_dir_ptr	
	vcb->root_dir_ptr = createDirectory(DIRECTORY_ENTRIES, NULL);
	if (vcb->root_dir_ptr == NULL) return -1;

	// DirectoryEntry location start after FreeSpace loc
	vcb->root_loc = vcb->root_dir_ptr->block_location.startLoc;

	return 0;
	}
	
	
void exitFileSystem ()
	{
	// Write Volumn Control Block back to the disk
	if (LBAwrite(vcb, 1, 0) < 1){
		printf("Unable to write VCB to system!\n"); //write unsuccessed
	} 
	// Write updated free space map and VCB to disk; printf write failure
    writeFSToDisk(vcb->fs_st.curExtentLBA);

	freePtr(vcb->fs_st.terExtTBMap, "Tetiary Table");
	freePtr(vcb->free_space_map, "Free Space");
	
	freePtr(vcb->root_dir_ptr, "Directory Entry");
	freePtr(vcb, "Volume Control Block");

	printf ("System exiting\n");
	}