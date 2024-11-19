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
extent_st* initFreeSpace(int numberOfBlocks, int blockSize) {
    vcb->free_space_loc = FREESPACE_START_LOC;
    vcb->fs_st.curExtentLBA = FREESPACE_START_LOC;
  
    vcb->fs_st.reservedBlocks = calBlocksNeededFS( numberOfBlocks, blockSize );
    vcb->fs_st.maxExtent = vcb->fs_st.reservedBlocks * PRIMARY_EXTENT_TB;

    // this is the index of the free space map
    vcb->fs_st.extentLength = 0;

    // Calculate start location of free blocks on disk
    int startFreeBlockLoc = vcb->fs_st.reservedBlocks + vcb->free_space_loc;
    
    vcb->fs_st.totalBlocksFree = numberOfBlocks - startFreeBlockLoc; // all available blocks
    
    // Init primary extent map for fspace
    extent_st* extentTable = (extent_st*) allocateMemFS(vcb->fs_st.reservedBlocks);
    
    extentTable[0] = (extent_st) { startFreeBlockLoc, vcb->fs_st.totalBlocksFree };
    vcb->fs_st.extentLength++;

    vcb->fs_st.terExtLength = 0; 
    vcb->fs_st.terExtTBLoc = -1; // Indicate tertiary table is not exist

    return extentTable;
}

/** Loads the fs map from disk into memory and retain the map in memory until the 
 * program terminates.
 * @return extent_st* on success or NULL on error
 */
extent_st* loadFreeSpaceMap(int startLoc) {
    // Prevent multiple reads of the same free space map from disk
    if (vcb->free_space_map && startLoc == vcb->fs_st.curExtentLBA) {
        return vcb->free_space_map;
    }
    
    // Allocate memory for the free space map
    extent_st* extentTable = (extent_st*) allocateMemFS(vcb->fs_st.reservedBlocks);

    // Read blocks into memory; release FS Map on failure
    int readStatus = LBAread(extentTable, vcb->fs_st.reservedBlocks, startLoc);
    if (readStatus < vcb->fs_st.reservedBlocks) {
        freePtr((void**) &extentTable, "extentTable");
        return NULL;
    }

    // Set current opend FreeMap in memory based on its start location LBA location
    vcb->fs_st.curExtentLBA = startLoc; 
    return extentTable;
}

/** Allocates a specified number of blocks from the free space map with a minimum
 * continuous block size, if available.
 * @return An extents_st struct with allocated extents, an empty extents_st if failure
 * @note Check extents != NULL before use.
 */
extents_st allocateBlocks(int nBlocks, int minContinuous) { 
    extents_st requestBlocks = { NULL, 0 };
    vcb->free_space_map = loadFreeSpaceMap(FREESPACE_START_LOC);

    // Check if request exceeds available blocks or does not meet minimum continuity
    int numBlockReq = (nBlocks > vcb->fs_st.totalBlocksFree) ? -1 : nBlocks;
    if (numBlockReq == -1 || nBlocks < minContinuous ) {
        printf("***************** Full Storage *****************\n");
        return requestBlocks;
    }

    // Estimate number of extents will allocate in memory
    int estExtents = min(nBlocks, vcb->fs_st.extentLength);

    // Allocate memory based on number of extents
    requestBlocks.extents = malloc(sizeof(extent_st) * estExtents);
    if (requestBlocks.extents == NULL) return requestBlocks;
    
    requestBlocks.size = 0;
    
    // Iterate over free space map extents to allocate blocks
    for (int i = (vcb->fs_st.extentLength - 1); i >= 0 && numBlockReq > 0; i--) {
        
        int index = i % vcb->fs_st.maxExtent;
        int indexTable = i / vcb->fs_st.maxExtent - 1;
        
        // Load a secondary table when needed.
        pageSwap(index, indexTable);
        
        int startLocation = vcb->free_space_map[index].startLoc;
        int availableBlocks = vcb->free_space_map[index].countBlock; 
        
        if (availableBlocks < minContinuous || startLocation == -1) continue;

        // if number of blocks request less than or equal count, insert the pos & count
        // to extent list, and then remove this extent in primary table.
        if (availableBlocks <= numBlockReq) {

            // Add a new extent with location and count 
            requestBlocks.extents[requestBlocks.size++] = (extent_st) { startLocation, \
                                                            availableBlocks };
            // Adjust the total free blocks of freespace map
            vcb->fs_st.totalBlocksFree -= availableBlocks;
            
            // Remove this extent from table
            removeExtent(startLocation, index);
            numBlockReq -= availableBlocks;  // Reduce number of blocks requested

        } else {
            // Add a new extent with location and count
            requestBlocks.extents[requestBlocks.size++] = (extent_st)\
                                {vcb->free_space_map[index].startLoc, numBlockReq};
           
            // Update the extent in free space map to reduce its count
            vcb->free_space_map[index].startLoc += numBlockReq;
            vcb->free_space_map[index].countBlock -= numBlockReq;
            vcb->fs_st.totalBlocksFree -= numBlockReq;
            
            numBlockReq = 0; // All required blocks have been assigned
        }
    }

    // If unable to fulfill request due to fragmentation, release allocated blocks
    if (numBlockReq > 0){
        printf("--------- ERROR - Unable to allocate blocks ---------\n");
        freeExtents(&requestBlocks);
        return requestBlocks;
    }

    // Reduce the size of memory before return
    requestBlocks.extents = realloc(requestBlocks.extents, requestBlocks.size * sizeof(extent_st) );
    if (requestBlocks.extents == NULL) {
        printf("--------- ERROR - Unable to reallocate memory --------- \n");
    }
    writeFSToDisk(vcb->fs_st.curExtentLBA);
    return requestBlocks;
}

/** Releases blocks starting from a specified location by iterates through the 
 * free space map to check if an extent can be merged before adding it to the map. 
 * NOTE: check for overlapping extents (processing... )
 * @return -1 if fail or 0 is sucessed 
 */
int releaseBlocks(int startLoc, int countBlocks) {
    // If the specified range exceeds total blocks, return -1 if error
	vcb->free_space_map = loadFreeSpaceMap(FREESPACE_START_LOC);
    
    int mergeLoc = startLoc + countBlocks;

    if (mergeLoc > vcb->total_blocks) return -1;
    
    int isNotFound = 1;

    // Iterate through the free space map to find a matching extent for merging
    for (int i = 0; i < vcb->fs_st.extentLength; i++) {
        
        int index = i % vcb->fs_st.maxExtent;
        int indexTable = i / vcb->fs_st.maxExtent - 1;
        
        int checkOverlap = isOverlap(vcb->free_space_map[index], startLoc, countBlocks); 
        if (checkOverlap == -1) return -1;
        // Load a secondary table when needed.
        pageSwap(index, indexTable);
        
        // Merge if matching extent is found, update location and block count.
        if ( mergeLoc == vcb->free_space_map[index].startLoc ) {
            vcb->free_space_map[index].startLoc -= countBlocks;
            vcb->free_space_map[index].countBlock += countBlocks;
            isNotFound = 0;
            break;
        }
        
        // If there is a spot available [-1: 0], replace its with an extent
        if (vcb->free_space_map[index].startLoc == -1) {
            vcb->free_space_map[index].startLoc = startLoc;
            vcb->free_space_map[index].countBlock = countBlocks;
            isNotFound = 0;
            break;
        }
    }

    // Add a new extent to FS map if no matching merge location is found
    if ( isNotFound ) addExtent(startLoc, countBlocks);
    vcb->fs_st.totalBlocksFree += countBlocks; // Update total free blocks

    // printf("====RELEASED [%d: %d] - Status: OK======\n", startLoc, countBlocks);
    return writeFSToDisk(vcb->fs_st.curExtentLBA);
}

// Adds a new extent to the fs map, specifying starting location and block count.
int addExtent(int startLoc, int countBlock) {
    int index = indexExtentTB();
    // If only Primary table exist
    if (vcb->fs_st.extentLength < vcb->fs_st.maxExtent) {
        vcb->free_space_map[vcb->fs_st.extentLength].startLoc = startLoc;
        vcb->free_space_map[vcb->fs_st.extentLength].countBlock = countBlock;
        vcb->fs_st.extentLength++;
        return 0;
    }

    // Handle cases when we need secondary extent table 1024 % 1024 == 0
    if ( index == 0 ) {
        /** Create new secondary Extent TB */
        int status = createSecondaryExtentTB(startLoc, countBlock);
        if (status == -1) { 
            printf("/** Create new secondary Extent TB */\n");
            return -1;
        }
    }

    /** 
     * Open the secondary extent table to add an extent:
     * - Retrieve the secondary extent table using the index
     * - Load the secondary extent table into memory based on its location 
     * - Set the extent and increase the extent length.
     */
    int statusSec = secondaryTBIndex();
    if (statusSec == -1 ) return -1;
    int secondaryTBLoc = getSecTBLocation( statusSec );
    
    vcb->free_space_map = loadFreeSpaceMap(secondaryTBLoc);

    vcb->free_space_map[index].startLoc = startLoc;
    vcb->free_space_map[index].countBlock = countBlock;
    vcb->fs_st.extentLength++;
    
    return 0;
    
}
/** Create tertiary extent table return its status */
int createTertiaryExtentTB(){
    /** Check if third extent is not exists; 
     *   - Allocate 1 block on disk for tertiary table
     *   - Retrieve location for third extent
     *   - // Skipped - Add [3rd Loc: -3] to primary extent.
     * else: tertiary table exist:
     *   - Load tertiary table to memory
     */
    if ( vcb->fs_st.terExtTBLoc == -1 ) {
        printf("Created Tertiary extent table!\n");
        vcb->fs_st.terExtTBLoc = createExtentTables(1,0);

        if ( vcb->fs_st.terExtTBLoc == -1 ) return -1;
        printf("Created Tertiary extent table. Success!!!!!!!!! \n");
    }

    return loadTertiaryTB();
}

/** Check if TertiaryTB exist in memory, if not load to memory */
int loadTertiaryTB() {
    if (!vcb->fs_st.terExtTBMap) {
        vcb->fs_st.terExtTBMap = (int*) allocateMemFS(1);

        int readStatus = LBAread(vcb->fs_st.terExtTBMap, 1, vcb->fs_st.terExtTBLoc);
        if (readStatus < 1) return -1;
    } printf("LOADED Tertiary Ext Table to Memory\n");
    return 0;
}

// Create Secondary Extent and return its status
int createSecondaryExtentTB() {
    /** Create a secondary extent table:
     *   - Allocate 16 contiguou blocks for secondary table.
     *   - Get the location of the secondary extent table.
     *   - Update third extent with location of secondary extent table and increase its size.
     *   - Write the third extent to disk (finalize memory changes).
     *   - Load secondary extent table into filesystem map pointer.
     *   - // Skipped - Update last extent in primary extent to link to the secondary table and increase its length.
     *   - Add the new start and count to the secondary table and increase its extent length.
     */
    /* If 3rd TB not exist, create it */
    int status = createTertiaryExtentTB();
    if (status == -1) return -1;

    int secondTBLoc = createExtentTables(vcb->fs_st.reservedBlocks, vcb->fs_st.reservedBlocks);
    if (secondTBLoc == -1) {
        printf("Error - Can not create secondary extent table\n");
        return -1;
    }

    // Add second extent table location to tertiary extent table map
    vcb->fs_st.terExtTBMap[vcb->fs_st.terExtLength++] = secondTBLoc;
    
    // Write updated Tertiary extent table to disk 
    int writeStatus = LBAwrite(vcb->fs_st.terExtTBMap, 1, vcb->fs_st.terExtTBLoc);
    if (writeStatus == -1) return -1;

    printf("Created secondary extent table - SUCCESS!!!\n");
    return 0;
}

/** Allocate blocks for Primary, Secondary or Tertiary Extent
 * @return start location; -1 on error 
 */
int createExtentTables(int nBlocks, int nContiguous) {
    extents_st aBlocks = allocateBlocks(nBlocks, nContiguous);

    if ( aBlocks.extents == NULL || aBlocks.size == 0) {
        printf("Error - Unable to create new Extent Tables with %d blocks and %d blocks are \
                                        contiguous\n", nBlocks, nContiguous);
        return -1;
    }
    int extLoc = aBlocks.extents[0].startLoc;
    freeExtents(&aBlocks);
    return extLoc;
}

// @return the remainder of extentLength divided by maxExtent
int indexExtentTB() { // Example: 1023 % 1024 -> 1023
    return vcb->fs_st.extentLength % vcb->fs_st.maxExtent;
}

// @return index of the secondary table by dividing extentLength by maxExtent,
// and subtracting 1 to get the secondary table index
// Example: 1024 / 1024 - 1 -> 0 (This gives the index of the secondary table)
int secondaryTBIndex() {
    return vcb->fs_st.extentLength / vcb->fs_st.maxExtent - 1;
}

/** Remove an extent_st from the FreeSpace based on its location
 * Instead of remove an extent from the list by looping for all extents of the table,
 * temporary set this extent as [-1 : 0] mark as reserve space in the table. */ 
void removeExtent( int startLoc, int i ) {
    vcb->free_space_map[i].startLoc = -1;
    vcb->free_space_map[i].countBlock = 0;
}

/** Check if current index is on the new extent table, change the table */
void pageSwap(int idx, int idxPage) {
    // Load a secondary table when needed.
    if (idx == 0 && idxPage > -1) {
        writeFSToDisk(vcb->fs_st.curExtentLBA);            

        int secTBLoc = getSecTBLocation(idxPage);
        vcb->free_space_map = loadFreeSpaceMap(secTBLoc);
    }
}

/** Retrieves the location of a secondary extent table by its index
 * Loads the tertiary translation table first. 
 * @return the location if successful, or -1 if error
 */
int getSecTBLocation(int secIdx) {
    int status = loadTertiaryTB();
    if (status != 0) {
        printf("ERROR - getSecTBLocation @ %d\n", secIdx);
        return -1;
    }
    return vcb->fs_st.terExtTBMap[secIdx];
}
/** Write the updated free space map and Volume Control Block (VCB) to disk 
 * when the user terminates the program. This also applies when allocating 
 * or releasing blocks from different tables */
int writeFSToDisk(int startLoc) {
    int wCount = LBAwrite (vcb->free_space_map, vcb->fs_st.reservedBlocks, startLoc);
    if (wCount != vcb->fs_st.reservedBlocks) {
        printf("ERROR - writeFSToDisk @ %d - wCount: %d - reservedBlocks: %d\n", startLoc, wCount, vcb->fs_st.reservedBlocks);
        return -1;
    }
    // printf("Saved Free Space Map to disk @ %d ... \n", startLoc);   
    return 0;
}

/** Reserve the minimum number of blocks required for free space by considers 
 * fragmentation and the size of each block.
 * @return estimated number of blocks, ensuring at least one block is allocated
 */
int calBlocksNeededFS(int blocks, int blockSize) {
    // Estimate number of extents based on fragmentation percentage
    int estNumOfExtents = blocks * FRAGMENTATION_PERCENT;
    int extentsSize = estNumOfExtents * sizeof(extent_st); // 8 bytes per extent

    // calculate number of blocks need for freespace base on BlockSize
    int blocksNeeded = computeBlockNeeded(extentsSize, blockSize); 
    return (blocksNeeded < 1) ? 1 : blocksNeeded;
}

void* allocateMemFS(int nBlocks) {
    void* ptr = malloc(nBlocks * vcb->block_size);
    if (!ptr) {
        printf("Unable to allocate memory -- allocateMemFS --\n");
        return NULL;
    } return ptr;
}

/** Check if the sub-range is outside the existing range
 * @return 0 on success or -1 if is overlap
*/
int isOverlap(extent_st existExt, int addExtStart, int addExtCount) {
    int end = existExt.startLoc + existExt.countBlock - 1;
    int subEnd = addExtStart + addExtCount - 1;

    if (subEnd < existExt.startLoc || addExtStart > end) {
        return 0;  // Outside
    }
    printf("--- WARNING: Extent is overlap, check your system ---\n");
    return -1;
}


//Free memory allocated for an extents_st
void freeExtents(extents_st *reqBlocks) {
    if (reqBlocks && reqBlocks->extents) {
        freePtr((void **) &reqBlocks->extents, "blocks allocate");
        reqBlocks->extents = NULL;
    }
    if (reqBlocks) reqBlocks->size = 0;
}

// Release array of extents back to freespace map
void returnExtents(extents_st returnExt) {
    if (returnExt.size == 0 || !returnExt.extents) return;

    for (size_t i = 0; i < returnExt.size; i++) {
        releaseBlocks(returnExt.extents[i].startLoc, returnExt.extents[i].countBlock);
    }
    freeExtents(&returnExt);
}