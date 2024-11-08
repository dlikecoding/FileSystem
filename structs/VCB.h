/**************************************************************
* Class::  CSC-415-03 FALL 2024
* Name:: Danish Nguyen
* Student IDs:: 923091933
* GitHub-Name:: dlikecoding
* Group-Name:: 0xAACD
* Project:: Basic File System
*
* File:: VCB.h
*
* Description:: Volume Control Block (VCB) structure, containing fields 
* for volume management and pointers for runtime operations.
*
**************************************************************/

#ifndef _VCB_H
#define _VCB_H


#include "structs/FreeSpace.h"
#include "structs/DE.h"

/* Volume Control Block contains both persistent fields (stored on disk) and 
runtime-only pointers */ 
typedef struct volume_control_block {
    // Fields stored on disk
    char volume_name[MAX_FILENAME]; // human-readable name for the volume

    unsigned long signature;    // signature for validation of the volume (LBA)
    
    unsigned int total_blocks;  // total number of blocks in the LBA
    unsigned int block_size;    // size of one block in bytes
    unsigned int root_loc;      // location of block containing the directory
    unsigned int free_space_loc;// start location of the free space block on disk

    freespace_st fs_st;         // fs structure store fields to manage freespace

    // Pointers for Runtime-only (NOT WRITTEN TO DISK)
    extent_st* free_space_map;     // pointer to free space map
    directory_entry* root_dir_ptr; // pointer to the root directory

    directory_entry* cwdLoadDE; // Pointer to the directory entry struct for the cwd
    char* cwdStrPath;            // Pointer to the string representation of the cwd path
    
} volume_control_block;

extern volume_control_block* vcb; // Global declaration of the volume control block
#endif


