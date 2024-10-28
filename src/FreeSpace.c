/**************************************************************
* Class::  CSC-415-03 FALL 2024
* Name:: Danish Nguyen
* Student IDs:: 923091933
* GitHub-Name:: dlikecoding
* Group-Name:: 0xAACD
* Project:: Basic File System
*
* File:: FreeSpace.h
*
* Description:: Managing the free space map in a filesystem. The 
* freespace_st structure tracks available blocks on disk, fs map location
* store on disk, length of extents, total blocks reserved in FreeSpace and 
* fragmentation levels. It also contains flags indicating the status of 
* secondary and tertiary extents
*
**************************************************************/

#include "structs/VCB.h"
#include "structs/FreeSpace.h"

/**
 * Initializes the free space map on the first time. Set location and reserving blocks.
 * - Calculates blocks needed for free space and configures extents for disk management.
 * - Writes initial extent structure to disk, starting at the designated free space location.
 * @return -1 on failure, 0 on success.
 */
int initFreeSpace(int numberOfBlocks, int blockSize) {
    vcb->fs_st.freeSpaceLoc = FREESPACE_START_LOC;
    vcb->fs_st.reservedBlocks = calBlocksNeededFS( numberOfBlocks, blockSize, \
                                                        FRAGMENTATION_PERCENT );
    vcb->fs_st.maxExtent = vcb->fs_st.reservedBlocks * PRIMARY_EXTENT_TB;

    // Calculate start location of free blocks on disk
    int startFreeBlockLoc = vcb->fs_st.reservedBlocks + vcb->fs_st.freeSpaceLoc;
    
    vcb->fs_st.totalBlocksFree = numberOfBlocks - startFreeBlockLoc; // all available blocks
    
    // Init first extent map for fspace
    extent_st extentMap = { startFreeBlockLoc, vcb->fs_st.totalBlocksFree };
    vcb->fs_st.extentLength++;
    vcb->fs_st.lastExtentSize = vcb->fs_st.totalBlocksFree;
    
    // Write extent structure to disk and return -1 if failure
	if (LBAwrite((char*) &extentMap, 1, 1) != 1) return -1; 
    return 0;
}

/** Loads the free space map from disk to memory
 * @return -1 on error, or the number of blocks loaded
*/
int loadFSMap() {
    // Determine the blocks to read based on extent length
    int nblocksRead = computeBlockNeeded(vcb->fs_st.extentLength, PRIMARY_EXTENT_TB);

    /* if blocks read > reservedBlocks, retrieve the last extent element and 
    also load secondary/tertiary table.

    For now, loaded primary extent table to memory. */
    int actualBlocksRead = (nblocksRead > vcb->fs_st.reservedBlocks) ? \
                        vcb->fs_st.reservedBlocks : nblocksRead;

    // Allocate memory for the free space map
    vcb->free_space_map = malloc(actualBlocksRead * vcb->block_size);
    if (vcb->free_space_map == NULL) return -1;

    // Read blocks into memory; release FS Map on failure
    int readStatus = LBAread(vcb->free_space_map, actualBlocksRead, 1);
    if (readStatus < actualBlocksRead) {
        releaseFSMap();
        return -1;
    }
    return actualBlocksRead;
}



/** Allocates a specified number of blocks from the free space map with a minimum
 * continuous block size, if available.
 * @return An `extents_st` struct with allocated extents or an empty struct if failure */
extents_st allocateBlocks(int nBlocks, int minContinuous) { 
    extents_st requestBlocks = { NULL, 0 };

    // Check if request exceeds available blocks or does not meet minimum continuity
    int numBlockReq = (nBlocks > vcb->fs_st.totalBlocksFree) ? -1 : nBlocks;
    if (numBlockReq == -1 || nBlocks < minContinuous ) return requestBlocks;
    
    int loadedBlocks = loadFSMap(); // Load the free space map into memory
    if ( loadedBlocks == -1 ) return requestBlocks;
    
    int maxFreeBlocks = loadedBlocks * PRIMARY_EXTENT_TB;

    /* NOTE: In the worst case scenarios, length of return extents could equal or 
    greater than length of extents in free space map */
    
    // Allocate space for the returned extents
    requestBlocks.extents = malloc(sizeof(extent_st) * maxFreeBlocks);
    if (requestBlocks.extents == NULL) {
        releaseFSMap();
        return requestBlocks;
    }
    requestBlocks.size = 0;

    // Iterate over free space map extents to allocate blocks
    for (int i = 0; i < vcb->fs_st.extentLength && numBlockReq > 0; i++) {
        int startLocation = vcb->free_space_map[i].startLoc;
        int availableBlocks = vcb->free_space_map[i].countBlock; 
            
        printf("FS Loop: [%d: %d]\n", startLocation, availableBlocks);

        if (availableBlocks < minContinuous) continue;

        // if number of blocks request less than or equal count, insert the pos & count
        // to extent list, and then remove this extent in primary table.
        if (availableBlocks <= numBlockReq) {

            // Add a new extent with location and count 
            requestBlocks.extents[requestBlocks.size++] = (extent_st) { startLocation, \
                                                            availableBlocks };
            // Adjust the total free blocks of freespace map
            vcb->fs_st.totalBlocksFree -= availableBlocks;
            
            // Remove this extent from table and adjust loop index
            removeExtent(startLocation); i--;
            numBlockReq -= availableBlocks;  // Reduce number of blocks requested

        } else {
            // Add a new extent with location and count
            requestBlocks.extents[requestBlocks.size++] = (extent_st)\
                                {vcb->free_space_map[i].startLoc, numBlockReq};
            
            // Update the extent in free space map to reduce its count
            vcb->free_space_map[i].startLoc += numBlockReq;
            vcb->free_space_map[i].countBlock -= numBlockReq;
            vcb->fs_st.totalBlocksFree -= numBlockReq;
            numBlockReq = 0; // All required blocks have been assigned
        }
        printf("ffound: [%d: %d] %d\n", startLocation, availableBlocks, numBlockReq);
    }
    
    // If unable to fulfill request due to fragmentation, release allocated blocks
    if (numBlockReq > 0) {
        releaseReqBlocks(requestBlocks);
        releaseFSMap();
        return requestBlocks;
    }

    // Write updated free space map and VCB to disk; handle write failure
    if (LBAwrite(vcb->free_space_map, loadedBlocks, 1) < loadedBlocks) {
        releaseReqBlocks(requestBlocks); 
        return requestBlocks;
    }
    if (LBAwrite(vcb, 1, 0) < 1) return requestBlocks;
    releaseFSMap(); // Release free space map in memory

    return requestBlocks;
}


/** Releases blocks starting from a specified location by iterates through the 
 * free space map to check if an extent can be merged before adding it to the map. 
 * NOTE: check for overlapping extents (processing... )
 * @return -1 if fail or 0 is sucessed */
int releaseBlocks(int startLoc, int nBlocks) {
    // If the specified range exceeds total blocks, return -1 if error
    if (startLoc + nBlocks > vcb->total_blocks) return -1;
    
    // Load the free space map, return -1 if loading fails
    int loadedBlocks = loadFSMap();
    if ( loadedBlocks == -1 ) return -1;
    
    int mergeLoc = startLoc + nBlocks;
    int isNotFound = -1;

    // Iterate through the free space map to find a matching extent for merging
    for (size_t i = 0; i < vcb->fs_st.extentLength; i++) {
        
        // Merge if matching extent is found, update location and block count.
        if ( mergeLoc == vcb->free_space_map[i].startLoc ) {
            vcb->free_space_map[i].startLoc -= nBlocks;
            vcb->free_space_map[i].countBlock += nBlocks;
            isNotFound = 0; break;
        }
    }

    // Add a new extent to FS map if no matching merge location is found
    if ( isNotFound ) {
        addExtent(startLoc, nBlocks);
        vcb->fs_st.lastExtentSize = nBlocks;
    }
    vcb->fs_st.totalBlocksFree += nBlocks; // Update total free blocks

    // Write updated FS map and VCB to disk, return -1 if writing fails.
    if (LBAwrite(vcb->free_space_map, loadedBlocks, 1) < loadedBlocks) return -1;
    if (LBAwrite(vcb, 1, 0) < 1) return -1;

    // Release resources associated with the free space map.
    releaseFSMap();
    
    return 0;
}

void releaseFSMap() {
    free(vcb->free_space_map);
    vcb->free_space_map = NULL;
}

void releaseReqBlocks(extents_st reqBlocks) {
    free(reqBlocks.extents);
    reqBlocks.extents = NULL;
    reqBlocks.size = 0;
}

void addExtent(int startLoc, int countBlock) {
    vcb->free_space_map[vcb->fs_st.extentLength].startLoc = startLoc;
    vcb->free_space_map[vcb->fs_st.extentLength].countBlock = countBlock;
    vcb->fs_st.extentLength++;
}

// Remove an extent_st structure from the FreeSpace based on its location
void removeExtent( int startLoc ) {
    for (int i = 0; i < vcb->fs_st.extentLength; i++) {
        if (vcb->free_space_map[i].startLoc == startLoc) {
            // shift remaining extents to the left
            for (int j = i; j < vcb->fs_st.extentLength - 1; j++) {
                vcb->free_space_map[j] = vcb->free_space_map[j + 1];
            }
            vcb->fs_st.extentLength-- ;
        }
    }
}


/** Reserve the minimum number of blocks required for free space.
 * @return 1 is minimum or number of blocks needed */
int calBlocksNeededFS(int blocks, int blockSize, double fragmentPerc) {
    // Estimate the number of extents based on fragmentation percentage
    int estExtents = (int)(blocks * fragmentPerc);
    
    // 8 bytes per extent
    int extentsSize = estExtents * sizeof(extent_st);

    // calculate number of blocks need for freespace base on BlockSize
    int blocksNeeded = computeBlockNeeded(extentsSize, blockSize); 
    return (blocksNeeded < 1) ? 1 : blocksNeeded;
}