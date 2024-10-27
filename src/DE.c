/**************************************************************
* Class::  CSC-415-03 FALL 2024
* Name:: Danish Nguyen
* Student IDs:: 923091933
* GitHub-Name:: dlikecoding
* Group-Name:: 0xAACD
* Project:: Basic File System
*
* File:: DE.c
*
* Description:: Directory Entry is a data structure that stores metadata for 
* a file or directory in the filesystem. It includes information such as creation, 
* modification, and access timestamps; file size; entry type (file or directory); 
* usage status; file name; and an array of extents that specify the locations of 
* data blocks on disk
*
**************************************************************/

#include "structs/DE.h"

/** Initializes a new directory structure in memory with a specified number of entries as 
 * a subdirectory of a given parent directory. It calculates required space, allocates memory,
 * sets up initial entries for current (".") and parent ("..") links, and writes the directory 
 * to disk.
 * @return a pointer to the created directory or NULL if failure */
directory_entry *create_directory(int numEntries, directory_entry *parent) {

    // Calculate memory needed for dir entries based on count and block size
    int bytesNeeded = numEntries * sizeof(directory_entry);
    int blocksNeeded = computeBlockNeeded(bytesNeeded, BLOCK_SIZE);
    int actualBytes = blocksNeeded * BLOCK_SIZE;
    int actualEntries = actualBytes / sizeof(directory_entry);

    // Allocate memory for new directory entries
    directory_entry *newDirectory = (directory_entry *)malloc(actualBytes);
    if (newDirectory == NULL) return NULL;

    // Get free blocks on disk location from FS map for this directory
    extent_st* blocksLoc = allocateBlocks(blocksNeeded, blocksNeeded).extents;
    if (blocksLoc == NULL) {
        free(newDirectory);
        return NULL;
    }

    extent_st blockLocation = blocksLoc[0];

    // Initialize ifself and parent directory entries
    for (int i = 2; i < actualEntries; i++) {
        newDirectory[i].is_used = 0;
        newDirectory[i].file_name[0] = UNUSED_ENTRY_MARKER;  // Mark entry as unused
    }

    time_t currentTime = time(NULL);

    // Initialize root directory entry "."
    createDirHelper(newDirectory[0], ".", blockLocation,
        actualEntries * sizeof(directory_entry), 1, 1, currentTime,
        currentTime, currentTime);

    // Initialize parent directory entry ".." - If no parent, point to self
    if (parent == NULL) parent = newDirectory;

    createDirHelper(newDirectory[1], "..", parent[0].block_location,
        parent[0].file_size, parent[0].is_directory, 1, parent[0].creation_time,
        parent[0].access_time, parent[0].modification_time);

    // Write created directory structure to disk
    int writeStatus = writeDirHelper(newDirectory);
    if (writeStatus == -1) {
        free(newDirectory);
        return NULL;
    }

    return newDirectory;
}

// Specified metadata, including name, location, size, type, usage status, and timestamps
void createDirHelper(directory_entry de, char *fName, extent_st loc, int fSize, int isDir, 
                                    int isUsed, time_t ctime, time_t mTime, time_t aTime) {
    
    strcpy(de.file_name, ".");
    de.block_location = loc;
    de.file_size = fSize;
    de.is_directory = isDir;
    de.is_used = isUsed;
    de.creation_time = ctime;
    de.access_time = ctime;
    de.modification_time = ctime;
}

/** Writes a directory to disk at the specified location. 
 * @return 0 on success or -1 on failure.*/
int writeDirHelper(directory_entry *newDir) {
    int blocks = computeBlockNeeded(newDir[0].file_size, BLOCK_SIZE);

    if (LBAwrite(newDir, blocks, newDir[0].block_location.startLoc) < blocks) return -1;
    return 0;
}


// initialize root directory entry "."
// strcpy(newDirectory[0].file_name, ".");
// newDirectory[0].block_location[0] = blocksLoc;
// newDirectory[0].file_size = actualEntries * sizeof(directory_entry);
// newDirectory[0].is_directory = 1;
// newDirectory[0].is_used = 1;
// newDirectory[0].creation_time = currentTime;
// newDirectory[0].access_time = currentTime;
// newDirectory[0].modification_time = currentTime;

// strcpy(newDirectory[1].file_name, "..");
// newDirectory[1].block_location[0] = parent[0].block_location[0];
// newDirectory[1].file_size = parent[0].file_size;
// newDirectory[1].is_directory = parent[0].is_directory;
// newDirectory[1].is_used = 1;
// newDirectory[1].creation_time = parent[0].creation_time;
// newDirectory[1].access_time = parent[0].access_time;
// newDirectory[1].modification_time = parent[0].modification_time;