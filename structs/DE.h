/**************************************************************
* Class::  CSC-415-03 FALL 2024
* Name:: Danish Nguyen, Atharva Walawalkar, Cheryl Fong
* Student IDs:: 923091933, 924254653, 918157791
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
#define MAX_EXTENTS 8    // Maximum number of extents

#define UNUSED_ENTRY '\0' // Marker for unused entries
#define DIRECTORY_ENTRIES 50 
//SIZE OF DE 40 + 32 + 64 = 136
typedef struct {
    time_t creation_time;         // creation time of the file or directory
    time_t modification_time;     // last modification time
    time_t access_time;           // last access time
    
    int file_size;                // size of the file or directory
    int is_directory;             // 1 if directory, 0 if file
    int is_used;                  // 1 if used, 0 if unused
    
    int ext_length;               // number of extents

    char file_name[MAX_FILENAME]; // File or directory name
    extent_st extents[MAX_EXTENTS]; // An of extent for data blocks (Processing... )
    
} directory_entry;

directory_entry* createDirectory(int numEntries, directory_entry *parent);

int writeDirHelper(directory_entry *newDir);
directory_entry* readDirHelper(int dirLoc);
directory_entry* loadDir(directory_entry* directoryEntry);

int removeDE(directory_entry *de, int idx);
int sizeOfDE (directory_entry* de);
#endif
