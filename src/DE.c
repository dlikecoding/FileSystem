#include "structs/DE.h"


// create a new directory with specified number of entries
directory_entry *create_directory(int numEntries, directory_entry *parent) {
    // calculate required space
    int bytesNeeded = numEntries * sizeof(directory_entry);
    int blocksNeeded = computeBlockNeeded(bytesNeeded, BLOCK_SIZE);
    int actualBytes = blocksNeeded * BLOCK_SIZE;
    int actualEntries = actualBytes / sizeof(directory_entry);

    // allocate memory for directory entries
    directory_entry *newDirectory = (directory_entry *)malloc(actualBytes);
    if (newDirectory == NULL) return NULL;

    // allocate free blocks for the directory from freespace map
    extent_st* blocksLoc = allocateBlocks(blocksNeeded, blocksNeeded).extents;
    if (blocksLoc == NULL) {
        free(newDirectory);
        return NULL;
    }

    extent_st blockLocation = blocksLoc[0];

    // initialize directory entries
    for (int i = 2; i < actualEntries; i++) {
        newDirectory[i].is_used = 0;
        newDirectory[i].file_name[0] = UNUSED_ENTRY_MARKER;  // Mark entry as unused
    }

    time_t currentTime = time(NULL);

    // initialize root directory entry "."
    createDirHelper(newDirectory[0], ".", blockLocation,
        actualEntries * sizeof(directory_entry), 1, 1, currentTime,
        currentTime, currentTime);

    // Initialize parent directory entry ".." - If no parent, default to self
    if (parent == NULL) parent = newDirectory;

    createDirHelper(newDirectory[1], "..", parent[0].block_location,
        parent[0].file_size, parent[0].is_directory, 1, parent[0].creation_time,
        parent[0].access_time, parent[0].modification_time);

    // Write directory to disk
    int writeStatus = writeDirHelper(newDirectory);
    if (writeStatus == -1) {
        free(newDirectory);
        return NULL;
    }

    return newDirectory;
}

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


// Function to write directory to disk (LBAwrite is assumed to be a provided function)
int writeDirHelper(directory_entry *newDir) {
    int blocks = computeBlockNeeded(newDir[0].file_size, BLOCK_SIZE);

    if (LBAwrite(newDir, blocks, newDir[0].block_location.startLoc) < 0) return -1;
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