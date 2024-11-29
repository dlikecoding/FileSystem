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
* Description:: This file system uses a Volume Control Block to 
* manage metadata, initialize storage, handle directories, and allocate 
* blocks. It detects existing systems or formats new ones. On termination, 
* it writes updates to disk and frees resources for a clean shutdown.
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

void volumeInfo(int nBlocks);
void displayExtentFS();
void displayRootDE();
void testFreeSpaceTertiary();


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
    
    // Initialize current working directory pointer
    vcb->cwdStrPath = malloc(1);
    if (!vcb->cwdStrPath) return -1;

    strcpy(vcb->cwdStrPath, "/");

    vcb->cwdLoadDE = NULL;

    // Signature is matched with current File System
    if (vcb->signature == SIGNATURE) {
		
        vcb->free_space_map = loadFreeSpaceMap(FREESPACE_START_LOC);
        vcb->root_dir_ptr = readDirHelper(vcb->root_loc);

		if (vcb->root_dir_ptr == NULL || vcb->free_space_map == NULL ) return -1;

        displayRootDE();
        volumeInfo(numberOfBlocks);
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

    volumeInfo(numberOfBlocks);
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


// Displays the file system's total capacity, available space, and used space in KB.
void volumeInfo(int nBlocks){ 
    printf("\n|-------- %s Info --------|\n", vcb->volume_name);
    printf("| %-11s | %12.2f KB   |\n", "Capacity", (double)(nBlocks * vcb->block_size) / 1024);
    printf("| %-11s | %12.2f KB   |\n", "Available", (double)(vcb->fs_st.totalBlocksFree * vcb->block_size) / 1024);
    printf("| %-11s | %12.2f KB   |\n", "Used", (double)((nBlocks - vcb->fs_st.totalBlocksFree) * vcb->block_size) / 1024);
}


// Tests allocation, release of blocks, and displays free space tables information.
// It also loads and prints tertiary extent table details.
void testFreeSpaceTertiary(){
    /* TEST ALLOCATE */
    extents_st test = allocateBlocks(19450, 0);

    /* TEST RELEASE */
    for (size_t id = 40; id < 10000 ; id++) { //1480 extents
        if (id % 2 != 0) {
            int test = releaseBlocks(id, 1);
            printf("============== currentA: %d (%ld, 1) =============\n", vcb->fs_st.curExtentLBA, id);
        }
    }

    int secondTab = getSecTBLocation(2);
    printf( "second Table loc: %d\n", secondTab);

    vcb->free_space_map = loadFreeSpaceMap(secondTab);
    for (size_t i = 0; i < 1000 ; i++) {
        printf("============== [ %d: %d ] =============\n", 
                        vcb->free_space_map[i].startLoc, 
                        vcb->free_space_map[i].countBlock);
    }
    
    /** Tertiary Extent Table */
    for (size_t i = 0; i < 10 ; i++) {
        printf("============== [ --- %d --- ] =============\n", vcb->fs_st.terExtTBMap[i]);
    }
}


void displayRootDE(){
    // directory_entry* newDir = createDirectory(DIRECTORY_ENTRIES, vcb->root_dir_ptr);
    // directory_entry* newDir = readDirHelper(42);
    
    // for (size_t i = 0; i < newDir->ext_length ; i++) { //vcb->fs_st.extentLength
    //     printf( "newDir: [%d: %d]\n", newDir->extents[i].startLoc, newDir->extents[i].countBlock);
    // }
    
    printf("%-11s %-10s %-7s %-4s %-4s %-4s %-7s\n",
       "Name", "Size", "LBA", "Used", "Type", "Ext", "Count");

    for (size_t i = 0; i < DIRECTORY_ENTRIES; i++){
        if (!vcb->root_dir_ptr[i].is_used) continue;
        printf("%-11s %-10d %-7d %-4d %-4d %-4d %-7d\n", 
            vcb->root_dir_ptr[i].file_name,
            vcb->root_dir_ptr[i].file_size,
            vcb->root_dir_ptr[i].extents->startLoc,
            vcb->root_dir_ptr[i].is_used,
            vcb->root_dir_ptr[i].is_directory,
            vcb->root_dir_ptr[i].ext_length,
            vcb->root_dir_ptr[i].extents->countBlock);
        
        // if (vcb->root_dir_ptr[i].ext_length < 2) continue;

        // for (size_t j = 0; j < vcb->root_dir_ptr[i].ext_length ; j++) {
        //     printf("EXT: [ %d | %d ]\n", vcb->root_dir_ptr[i].extents[j].startLoc, vcb->root_dir_ptr[i].extents[j].countBlock);
        // }
    }
}


void displayExtentFS() {
    for (size_t i = 0; i < vcb->fs_st.extentLength; i++){
        printf("FS: [%d: %d]\n", vcb->free_space_map[i].startLoc, vcb->free_space_map[i].countBlock);
    }
}
