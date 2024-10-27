#include "structs/VCB.h"
#include "structs/FreeSpace.h"

/**
 * Use on first inititalize VCB:
 * - Create freespace_st, calculate free blocks needed for FreeSpace
 *   on disk, and then write the Extent struct to 2nd block on the disk
 * - FreeSpace should start at 1 (after VCB)
 * @return -1 if fail, or 0 if success 
*/
int initFreeSpace(int numberOfBlocks, int blockSize) {
    vcb->fs_st.freeSpaceLoc = FREESPACE_START_LOC;
    vcb->fs_st.reservedBlocks = calBlocksNeededFS( numberOfBlocks, blockSize, \
                                                        FRAGMENTATION_PERCENT );
    vcb->fs_st.maxExtent = vcb->fs_st.reservedBlocks * PRIMARY_EXTENT_TB;

    // Example: 5 blocks for freespace, 1 block vcb => start at 6
    int startFreeLoc = vcb->fs_st.reservedBlocks + vcb->fs_st.freeSpaceLoc;
    
    vcb->fs_st.totalBlocksFree = numberOfBlocks - startFreeLoc; // 19531 - 6
    
    extent_st extentMap = { startFreeLoc, numberOfBlocks };
    vcb->fs_st.extentLength++;
    vcb->fs_st.lastExtentSize = numberOfBlocks;

    /* Test */
    // vcb->fs_st.totalBlocksFree = 3 + 4 + 102 + 5 + 16 + 50 + 78 + 300;
    // extent_st extentMap[] = { 
    //     { 19, 3 },{ 26, 4 }, { 392, 102 },
    //     { 44, 5 }, { 92, 16 }, { 192, 50 },
    //     { 299, 78 }, {1000, 300}
    // };
    // vcb->fs_st.extentLength = 8;
    // vcb->fs_st.lastExtentSize = 300;
    /* End */

    //write extents to disk unsuccessed
	if (LBAwrite((char*) &extentMap, 1, 1) != 1) return -1; 
    return 0;
}

/** Load blocks of freespace on disk to memory 
 * @return -1 if error, or block loaded
*/
int loadFSMap() {
    /* Calculate number of blocks to read from disk based on extentLength */
    int blockReadInLBA = computeBlockNeeded(vcb->fs_st.extentLength, PRIMARY_EXTENT_TB);

    /* if blocks read > reservedBlocks, retrieve the last extent element and 
    also load secondary/tertiary table.

    For now, loaded primary extent table to memory. */
    int actualBlocksRead = (blockReadInLBA > vcb->fs_st.reservedBlocks) ? \
                        vcb->fs_st.reservedBlocks : blockReadInLBA;

    vcb->free_space_map = malloc(actualBlocksRead * vcb->block_size);
    if (vcb->free_space_map == NULL) return -1;

    int readStatus = LBAread(vcb->free_space_map, actualBlocksRead, 1);
    if (readStatus < actualBlocksRead) {
        releaseFSMap();
        return -1;
    }
    return actualBlocksRead;
}



/** Get numbers of free block needed per request which has an array of 
 * extents and a size this this array.
 * @return a extents_st struct or NULL */ 
extents_st allocateBlocks(int nBlocks, int minContinuous) { 
    extents_st requestBlocks = { NULL, 0 };

    // request blocks needed greater than remaining space on disk, or 
    // blocks request is greater than min continuous, return requestBlocks.
    int numBlockReq = (nBlocks > vcb->fs_st.totalBlocksFree) ? -1 : nBlocks;
    if (numBlockReq == -1 || nBlocks < minContinuous ) return requestBlocks;
    
    int loadedBlocks = loadFSMap();
    if ( loadedBlocks == -1 ) return requestBlocks;
    
    int maxFreeBlocks = loadedBlocks * PRIMARY_EXTENT_TB;

    /* In the worst case scenarios, length of return extents could equal or 
    greater than length of extents in free space map */
    requestBlocks.extents = malloc(sizeof(extent_st) * maxFreeBlocks);
    if (requestBlocks.extents == NULL) {
        releaseFSMap();
        return requestBlocks;
    }
    requestBlocks.size = 0;


    for (int i = 0; i < vcb->fs_st.extentLength && numBlockReq > 0; i++) {
        int startLocation = vcb->free_space_map[i].startLoc;
        int availableBlocks = vcb->free_space_map[i].countBlock; 
        printf("***** [%d: %d] *****\n", startLocation, availableBlocks);
    
        if (availableBlocks < minContinuous) continue;

        // if number of block request greater or equal count, insert the pos & count
        // to extent list, and then remove this extent in primary table.
        if ( availableBlocks <= numBlockReq) {
            // add a new extent with location and count 
            requestBlocks.extents[requestBlocks.size++] = (extent_st) { startLocation, \
                                                            availableBlocks };

            // adjust the total free blocks of freespace map
            vcb->fs_st.totalBlocksFree -= availableBlocks;
            
            // remove this extent from table and adjust loop index
            removeExtent(startLocation); i--;
            
            // Reduce number of blocks requested
            numBlockReq -= availableBlocks;  

        } else {
            int reqStartLoc = vcb->free_space_map[i].startLoc + \
                                vcb->free_space_map[i].countBlock - numBlockReq;

            // add a new extent with location and count
            requestBlocks.extents[requestBlocks.size++] = (extent_st) {reqStartLoc, numBlockReq};
            
            // update the extent in free space map to reduce its count
            vcb->free_space_map[i].countBlock -= numBlockReq;
            vcb->fs_st.totalBlocksFree -= numBlockReq;
            
            numBlockReq = 0; // All required blocks have been assigned
        }
    }
    
    // if minContiguous is too high, contiguous blocks may not enough to fulfill for the request
    if (numBlockReq > 0) {
        releaseReqBlocks(requestBlocks);
        releaseFSMap();
        return requestBlocks;
    }


    for (size_t i = 0; i < vcb->fs_st.extentLength; i++) {
        printf(" [%d: %d] \n", vcb->free_space_map[i].startLoc, vcb->free_space_map[i].countBlock);
    }

    // write the extent table back to disk and close freespace map.
    if (LBAwrite(vcb->free_space_map, loadedBlocks, 1) < loadedBlocks) {
        releaseReqBlocks(requestBlocks); 
        return requestBlocks;
    }
    // write the vcb structure that contain fs map to 1st location on disk
    if (LBAwrite(vcb, 1, 0) < 1) return requestBlocks;
    releaseFSMap();

    return requestBlocks;
}
/**
 * Releases blocks starting from a specified location.
 * Iterates through the free space map to check if an extent can be merged
 * before adding it to the extent array. 
 * Note: CHECK for overlapping extents (processing... )
 * @return -1 if fail or 0 is sucessed */
int releaseBlocks(int startLoc, int nBlocks) {
    // Check if the specified range exceeds total blocks, return -1 if error
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

    // printf("\nRELEASE:\n");
    // for (size_t i = 0; i < vcb->fs_st.extentLength; i++) {
    //     printf("[%d: %d] \n", vcb->free_space_map[i].startLoc, vcb->free_space_map[i].countBlock);
    // }

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


// int findLBABlockNumber(int n, int nBlock) {
//     // for i in (array of Extent):
//     //     if (Extent.count > nBlock) {
//     //     return Extent.count + count //Actual LBA block number
//     //     } else {
//     //     count = count - Extent.count; //(5 - 3)
//     // }
//     return 1;
// }
