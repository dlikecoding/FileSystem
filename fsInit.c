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
#include "structs/DE.h"
#include "structs/FreeSpace.h"
#include "structs/VCB.h"

#define SIGNATURE 6565676850526897110

volume_control_block * vcb;

int initFileSystem (uint64_t numberOfBlocks, uint64_t blockSize)
{
    printf ("Initializing File System with %ld blocks with a block size of %ld\n", numberOfBlocks, blockSize);
    
    // Allocate first block on the disk into memory which store vcb struct
    vcb = malloc(blockSize);
    if (vcb == NULL) return -1;
    
    // Read first block on disk & return if error
    if (LBAread(vcb, 1, 0) < 1) return -1;
    
    vcb->free_space_map = NULL; 
    vcb->root_dir_ptr = NULL;
    vcb->fs_st.terExtTBMap = NULL;
    
    // Initialize current working directory
    vcb->cwdStrPath = malloc(1);
    if (!vcb->cwdStrPath) return -1;

    strcpy(vcb->cwdStrPath, "/");

    vcb->cwdLoadDE = NULL;

    // Signature is matched with current File System
    if (vcb->signature == SIGNATURE) {
		
        vcb->free_space_map = loadFreeSpaceMap(FREESPACE_START_LOC);
        vcb->root_dir_ptr = readDirHelper(vcb->root_loc);
        // vcb->cwdLoadDE = vcb->root_dir_ptr;

		if (vcb->root_dir_ptr == NULL || vcb->free_space_map == NULL ) return -1;

        for (size_t i = 0; i < vcb->fs_st.extentLength; i++){
            printf("FS: [%d: %d]\n", vcb->free_space_map[i].startLoc, vcb->free_space_map[i].countBlock);
        }

        printf("Name\t\tSize\t LBA   Used Type Ext  Count\n");
        for (size_t i = 0; i < 5; i++){
            printf("%s\t\t%-8d   %-5d %3d  %3d  %3d    %-5d\n", 
                vcb->root_dir_ptr[i].file_name,
                vcb->root_dir_ptr[i].file_size,
                vcb->root_dir_ptr[i].extents->startLoc,
                vcb->root_dir_ptr[i].is_used,
                vcb->root_dir_ptr[i].is_directory,
                vcb->root_dir_ptr[i].ext_length,
                vcb->root_dir_ptr[i].extents->countBlock);
        }

		// /* TEST ALLOCATE */
		// extents_st test = allocateBlocks(19450, 0);

		// /* TEST RELEASE */
		// for (size_t id = 40; id < 7000 ; id++) { //1480 extents
		// 	if (id % 2 != 0) {
		// 		int test = releaseBlocks(id, 1);
		// 		printf("============== currentA: %d (%ld, 1) =============\n", vcb->fs_st.curExtentLBA, id);
		// 	}
		// }

        // directory_entry* newDir = createDirectory(DIRECTORY_ENTRIES, vcb->root_dir_ptr);
        // directory_entry* newDir = readDirHelper(42);
        
        // for (size_t i = 0; i < newDir->ext_length ; i++) { //vcb->fs_st.extentLength
		// 	printf( "newDir: [%d: %d]\n", newDir->extents[i].startLoc, newDir->extents[i].countBlock);
		// }

		// int secondTab = getSecTBLocation(2);
		// printf( "second Table loc: %d\n", secondTab);

		// vcb->free_space_map = loadFreeSpaceMap(secondTab);
		// for (size_t i = 0; i < 1000 ; i++) { //vcb->fs_st.extentLength
		// 	printf("============== [ %d: %d ] =============\n", 
        //                     vcb->free_space_map[i].startLoc, 
        //                     vcb->free_space_map[i].countBlock);
		// }
		
        /** Tertiary Extent Table */
		// for (size_t i = 0; i < 10 ; i++) { //vcb->fs_st.extentLength
		// 	printf("============== [ --- %d --- ] =============\n", vcb->fs_st.terExtTBMap[i]);
		// }

        printf("\n --- Total Blocks Free: %d - Extent Length: %d - terExtTBLoc: %d --- \n", 
                            vcb->fs_st.totalBlocksFree, 
                            vcb->fs_st.extentLength, 
                            vcb->fs_st.terExtTBLoc);
        return 0;
    }

    // When sig not found - Initialize a new disk and reformat
    printf("Initialize a new disk and reformat ... \n");
    strcpy(vcb->volume_name, "FS-Project");
    vcb->signature = SIGNATURE;
    vcb->total_blocks = numberOfBlocks;
    vcb->block_size = blockSize;
    
    // Load the free space map into memory
    vcb->free_space_map = initFreeSpace(numberOfBlocks, blockSize);
    if (vcb->free_space_map == NULL) return -1;
    
    // Initialize root directory
    vcb->root_dir_ptr = createDirectory(DIRECTORY_ENTRIES, NULL);
    if (vcb->root_dir_ptr == NULL) return -1;
    
    // Initialize root LBA location
    vcb->root_loc = vcb->root_dir_ptr->extents[0].startLoc;
    
    // Initialize current working directory pointer
    // vcb->cwdLoadDE = vcb->root_dir_ptr;

    printf("\n --- Total Blocks Free: %d - Extent Length: %d - terExtTBLoc: %d --- \n", 
                            vcb->fs_st.totalBlocksFree, 
                            vcb->fs_st.extentLength, 
                            vcb->fs_st.terExtTBLoc);
    
    return 0;
}
	
void exitFileSystem ()
{
    // Write Volumn Control Block back to the disk
    if (LBAwrite(vcb, 1, 0) < 1){
        printf("Unable to write VCB to disk!\n");
    }

    // Write updated free space map to disk; print write failure
    if (writeFSToDisk(vcb->fs_st.curExtentLBA) == -1){
        printf("Unable to write free space map to disk!\n");
    }
    
    freePtr((void**) &vcb->fs_st.terExtTBMap, "Tetiary Table");
    freePtr((void**) &vcb->free_space_map, "Free Space");
    
    if (vcb->cwdLoadDE != vcb->root_dir_ptr){
        freePtr((void**) &vcb->cwdLoadDE, "CWD DE loaded");
    }
    freePtr((void**) &vcb->root_dir_ptr, "Directory Entry");
    freePtr((void**) &vcb->cwdStrPath, "CWD Str Path");

    freePtr((void**) &vcb, "Volume Control Block");
    
    printf ("System exiting\n");
}