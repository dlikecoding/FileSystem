/**************************************************************
* Class::  CSC-415-03 FALL 2024
* Name:: Danish Nguyen, Atharva Walawalkar, Arvin Ghanizadeh, Cheryl Fong
* Student IDs:: 923091933, 924254653, 922810925, 918157791
* GitHub-Name:: dlikecoding
* Group-Name:: 0xAACD
* Project:: Basic File System
*
* File:: DE.h
*
* Description:: Directory Entry is a data structure that stores metadata for 
* a file or directory in the filesystem. It includes information such as creation, 
* modification, and access timestamps; file size; entry type (file or directory); 
* usage status; file name; and an array of extents that specify the locations of 
* data blocks on disk
*
**************************************************************/
#ifndef DE_H
#define DE_H
#include <time.h>
#include "Extent.h"
#include "fs_utils.h"
#include "FreeSpace.h"  // Add this to get access to releaseBlocks

#define MAX_FILENAME 32
#define UNUSED_ENTRY '\0'
#define DIRECTORY_ENTRIES 50  

typedef struct directory_entry {
    time_t creation_time;      // Creation time of the file or directory
    time_t modification_time;  // Last modification time
    time_t access_time;        // Last access time
    
    int file_size;             // Size of the file or directory in bytes
    int is_directory;          // 1 if directory, 0 if file
    int is_used;               // 1 if used, 0 if unused
    
    char file_name[MAX_FILENAME]; // File or directory name
    
    extent_st* data_blocks;     // An array of extents store location of DE
} directory_entry;


// Public interface
directory_entry* createDirectory(int numEntries, directory_entry* parent);
directory_entry* loadDirectoryEntry(int rootLoc);

// Internal helper function declarations
int writeDirHelper(directory_entry* newDir);
// void releaseDirectory(directory_entry* dir);
// void releaseBlocksAndCleanup(extents_st blocksLoc);

#endif // DE_H