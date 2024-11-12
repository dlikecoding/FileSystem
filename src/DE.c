/**************************************************************
* Class::  CSC-415-03 FALL 2024
* Name:: Danish Nguyen, Atharva Walawalkar
* Student IDs:: 923091933, 924254653, 918157791
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
 * @author Danish Nguyen
 */
directory_entry *createDirectory(int numEntries, directory_entry *parent) {

    // Calculate memory needed for dir entries based on count and block size
    int bytesNeeded = numEntries * sizeof(directory_entry);
    int blocksNeeded = computeBlockNeeded(bytesNeeded, vcb->block_size);
    int actualBytes = blocksNeeded * vcb->block_size;
    int actualEntries = actualBytes / sizeof(directory_entry);

    // Retrieve available blocks on disk from fs map for this directory entry
    extents_st blocksLoc = (blocksLoc.extents == NULL) ? \
        allocateBlocks(blocksNeeded, blocksNeeded) : allocateBlocks(blocksNeeded, 2);

    // Our system does not support full disk fragmentation. If the number of extents exceeds 
    // MAX_EXTENTS by more than twice, it means the disk is full and no space available
    if ( blocksLoc.extents == NULL || blocksLoc.size > MAX_EXTENTS) {
        printf(" --- ERROR: Failed to allocate blocks for DE --- \n");
        
        // Return Blocks to Freespace on error
        returnExtents(blocksLoc);
        freeExtents(&blocksLoc);
        return NULL;
    }

    // Allocate memory for new directory entries
    directory_entry *newDir = (directory_entry *)malloc(actualBytes);
    if (newDir == NULL) return NULL;

    // Initialize self and parent directory entries
    for (int i = 2; i < actualEntries; i++) {
        newDir[i].is_used = 0;
        newDir[i].file_name[0] = UNUSED_ENTRY;  // Mark entry as unused
    }

    time_t currentTime = time(NULL);

    // Initialize root directory entry "."
    strcpy(newDir[0].file_name, ".");
    
    // Updated this to handle an array of extents
    memcpy(newDir[0].extents, blocksLoc.extents, blocksLoc.size * sizeof(extent_st));
    newDir[0].ext_length = blocksLoc.size;

    freeExtents(&blocksLoc); // release memory extent when done

    newDir[0].file_size = actualEntries * sizeof(directory_entry);
    newDir[0].is_directory = 1;
    newDir[0].is_used = 1;
    newDir[0].creation_time = currentTime;
    newDir[0].access_time = currentTime;
    newDir[0].modification_time = currentTime;

    // Initialize parent directory entry ".." - If no parent, point to self
    if (parent == NULL) parent = newDir;

    strcpy(newDir[1].file_name, "..");

    memcpy(newDir[1].extents, parent[0].extents, parent[0].ext_length * sizeof(extent_st));
    newDir[1].ext_length = parent[0].ext_length;

    newDir[1].file_size = parent[0].file_size;
    newDir[1].is_directory = parent[0].is_directory;
    newDir[1].is_used = 1;
    newDir[1].creation_time = parent[0].creation_time;
    newDir[1].access_time = parent[0].access_time;
    newDir[1].modification_time = parent[0].modification_time;
    
    // Write created directory structure to disk
    int writeStatus = writeDirHelper(newDir);
    if (writeStatus == -1) {
        freePtr(newDir, "DE DE.c");
        return NULL;
    }
    printf(" *** Successfully created DE - LBA @ %d *** \n ", newDir->extents[0].startLoc);

    return newDir;
}

/** Writes a directory to disk at the specified location. 
 * @return 0 on success or -1 on failure
 * @author Danish Nguyen, Atharva Walawalkar
 */
int writeDirHelper(directory_entry *newDir) {
    
    // if directory entries have continuous blocks
    if (newDir[0].ext_length == 1) {
        
        int blocks = computeBlockNeeded(newDir[0].file_size, vcb->block_size);

        if (LBAwrite(newDir, blocks, newDir[0].extents[0].startLoc) < blocks) {
            return -1;
        } return 0;
    }

    // If the block is not continuous, convert DE into a buffer and then write 
    // DEs to disk according to the number of blocks in each extent

    // create a buffer to fill all DEs on mem to disk
    char* newDirBlod = (char*) newDir;

    // Integrate through each extent in 'newDir' to full fill DEs
    for (int i = 0; i < newDir->ext_length; i++) {
        int startLoc = newDir[0].extents[i].startLoc;
        int countBlock = newDir[0].extents[i].countBlock;

        // write each extent block by block. Return -1 on failure
        if (LBAwrite(newDirBlod, countBlock, startLoc) < countBlock) {
            return -1;
        }
        // move cursor forward based on number of blocks written
        newDirBlod += (countBlock * vcb->block_size);
    } return 0;
}

/** Load root a directory to memory. 
 * @return 0 on success or -1 on failure
 * @author Danish Nguyen
 */
directory_entry* readDirHelper(int startLoc) {
    // Prevent multiple reads of the same LBA on disk
    if (vcb->cwdLoadDE && vcb->cwdLoadDE->extents[0].startLoc == startLoc) {
        return vcb->cwdLoadDE;
    }

    int blocks = computeBlockNeeded(DIRECTORY_ENTRIES * sizeof(directory_entry), vcb->block_size);
    
    // Allocate memory for the directory entries
    directory_entry* de = (directory_entry*)malloc(blocks * vcb->block_size);
    if (de == NULL) return NULL;

    // Read the first time to retrive the DE structure
    if (LBAread(de, blocks, startLoc) < blocks) {
        freePtr(de, "DE DE.c");
        return NULL;
    }

    if (de->ext_length == 1) return de; // Successfully loaded all DEs into memory
    
    /** Couldn't load all DEs into memory due to discontiguous, meaning 
    the extent length is greater than one. Filling the DE buffer by looping 
    through the extents */ 

    // create a buffer to fill all DEs on disk to mem
    char* dePtr = (char*) de; 
    
    for (int i = 0; i < de->ext_length; i++) {
        int startLoc = de->extents[i].startLoc;
        int countBlock = de->extents[i].countBlock;

        if (LBAread(dePtr, countBlock, startLoc) < countBlock) {
            freePtr(de, "DE DE.c");
            return NULL;
        }
        // move pointer to the next position in the buffer
        dePtr += (countBlock * vcb->block_size);
    }
    return de;
}


// Calculates number of directory entries.
int sizeOfDE (directory_entry* de) {
    return de[0].file_size / sizeof(directory_entry);
}