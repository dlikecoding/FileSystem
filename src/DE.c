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
* data blocks on disk (For now, only an extent in a DE)
*
**************************************************************/

#include "structs/DE.h"
#include "structs/VCB.h"

/** Initializes a new directory structure in memory with a specified number of entries 
 * as a subdirectory of a given parent directory. It calculates required space, allocates 
 * memory, sets up initial entries for current (".") and parent ("..") links, and writes 
 * the directory to disk.
 * @return a pointer to the created directory or NULL if failure 
 */
directory_entry* createDirectory(int numEntries, directory_entry *parent) {

    // Calculate memory needed for dir entries based on count and block size
    int bytesNeeded = numEntries * sizeof(directory_entry);
    int blocksNeeded = computeBlockNeeded(bytesNeeded, vcb->block_size);
    int actualBytes = blocksNeeded * vcb->block_size;
    int actualEntries = actualBytes / sizeof(directory_entry);

    // Allocate memory for new directory entries
    directory_entry *newDir = (directory_entry *)malloc(actualBytes);
    if (newDir == NULL) return NULL;

    // Retrieve available disk blocks from  fs map for allocating this directory.
    extents_st blocksLoc = allocateBlocks(blocksNeeded, blocksNeeded);
    if (blocksLoc.extents == NULL) {
        releaseExtents(blocksLoc);
        freePtr(newDir, "DE");
        return NULL;
    }

    /* TODO: Update this to handle multiple extents if needed, as future 
    versions may support arrays of extents. */
    extent_st blockLocation = blocksLoc.extents[0];
    releaseExtents(blocksLoc);
    
    // Initialize self and parent directory entries
    for (int i = 2; i < actualEntries; i++) {
        newDir[i].is_used = 0;
        newDir[i].file_name[0] = UNUSED_ENTRY;  // Mark entry as unused
    }

    time_t currentTime = time(NULL);

    // Initialize root directory entry "."
    strcpy(newDir[0].file_name, ".");
    newDir[0].block_location = blockLocation;
    newDir[0].file_size = actualEntries * sizeof(directory_entry);
    newDir[0].is_directory = 1;
    newDir[0].is_used = 1;
    newDir[0].creation_time = currentTime;
    newDir[0].access_time = currentTime;
    newDir[0].modification_time = currentTime;

    // Initialize parent directory entry ".." - If no parent, point to self
    if (parent == NULL) parent = newDir;

    strcpy(newDir[1].file_name, "..");
    newDir[1].block_location = parent[0].block_location;
    newDir[1].file_size = parent[0].file_size;
    newDir[1].is_directory = parent[0].is_directory;
    newDir[1].is_used = 1;
    newDir[1].creation_time = parent[0].creation_time;
    newDir[1].access_time = parent[0].access_time;
    newDir[1].modification_time = parent[0].modification_time;

    // Write created directory structure to disk
    int writeStatus = writeDirHelper(newDir);
    if (writeStatus == -1) {
        freePtr(newDir, "Directory Entry");
        return NULL;
    }
    return newDir;
}

/** Writes a directory to disk at the specified location. 
 * @return 0 on success or -1 on failure
 */
int writeDirHelper(directory_entry *newDir) {
    int blocks = computeBlockNeeded(newDir[0].file_size, vcb->block_size);

    if (LBAwrite(newDir, blocks, newDir[0].block_location.startLoc) < blocks) return -1;
    return 0;
}

/** Load root a directory to memory. 
 * @return 0 on success or -1 on failure
 */
directory_entry* loadDirectoryEntry() {
    int blocks = computeBlockNeeded(DIRECTORY_ENTRIES * sizeof(directory_entry), \
                                                                    vcb->block_size);
    directory_entry* de = malloc(blocks * vcb->block_size);
    if (LBAread(de, blocks, vcb->root_loc) < 1) return NULL;
    return de;
}
