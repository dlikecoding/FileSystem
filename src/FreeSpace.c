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

    vcb->fs_st.reservedBlocks = calBlocksNeededFS( numberOfBlocks, blockSize );
    vcb->fs_st.maxExtent = vcb->fs_st.reservedBlocks * PRIMARY_EXTENT_TB;

    // Calculate start location of free blocks on disk
    int startFreeBlockLoc = vcb->fs_st.reservedBlocks + vcb->free_space_loc;
    
    vcb->fs_st.totalBlocksFree = numberOfBlocks - startFreeBlockLoc; // all available blocks
    
    // Init primary extent map for fspace
    extent_st* extentTable = (extent_st*) allocateMemFS(vcb->fs_st.reservedBlocks);
    
    extent_st firstExt = { startFreeBlockLoc, vcb->fs_st.totalBlocksFree };
    extentTable[0] = firstExt;

    vcb->fs_st.extentLength++;
    vcb->fs_st.curExtentPage = -1;

    vcb->fs_st.tertiaryExtLength = 0;
    vcb->fs_st.terExtTBLoc = -1;

    // Write an extent structure to disk, return -1 if failure
	if (LBAwrite((char*) &firstExt, 1, FREESPACE_START_LOC) != 1) return NULL; 
    return extentTable;
}

/** Loads the whole fs map from disk into memory and retain the map in memory until the 
 * program terminates.
 * @return extent_st* on success or NULL on error
 */
extent_st* loadFreeSpaceMap(int startLoc) {
    printf("BEFORE******* curExtentPage: %d, tertiaryExtLength: %d, terExtTBLoc: %d \n", \
        vcb->fs_st.curExtentPage, vcb->fs_st.tertiaryExtLength, vcb->fs_st.terExtTBLoc); 
    
    if (vcb->free_space_map && startLoc == vcb->fs_st.curExtentPage) {
        return vcb->free_space_map;
    }
    // Allocate memory for the free space map
    extent_st* extentTable = (extent_st*) allocateMemFS(vcb->fs_st.reservedBlocks);

    // Read blocks into memory; release FS Map on failure
    int readStatus = LBAread(extentTable, vcb->fs_st.reservedBlocks, startLoc);
    if (readStatus < vcb->fs_st.reservedBlocks) {
        freePtr(extentTable, "extentTable\n");
        return NULL;
    }

    // Set current opend FreeMap in memory:
    vcb->fs_st.curExtentPage = startLoc;
    
    printf("AFTER******* curExtentPage: %d, tertiaryExtLength: %d, terExtTBLoc: %d \n", \
        vcb->fs_st.curExtentPage, vcb->fs_st.tertiaryExtLength, vcb->fs_st.terExtTBLoc); 
    
    return extentTable;
}

/** Allocates a specified number of blocks from the free space map with a minimum
 * continuous block size, if available.
 * @return An extents_st struct with allocated extents, an empty extents_st if failure
 * @note Check extents != NULL before use.
 */
extents_st allocateBlocks(int nBlocks, int minContinuous) { 
    extents_st requestBlocks = { NULL, 0 };

    // Check if request exceeds available blocks or does not meet minimum continuity
    int numBlockReq = (nBlocks > vcb->fs_st.totalBlocksFree) ? -1 : nBlocks;
    if (numBlockReq == -1 || nBlocks < minContinuous ) {
        printf("***************** Full Storage *****************\n");
        return requestBlocks;
    }

    // Estimate number of extents will allocate in memory based on number of block request
    int estExtents = (nBlocks < vcb->fs_st.extentLength) ? nBlocks : vcb->fs_st.extentLength;

    // Allocate space for the returned extents (512 * 16)
    requestBlocks.extents = malloc(sizeof(extent_st) * estExtents);
    if (requestBlocks.extents == NULL) return requestBlocks;
    
    requestBlocks.size = 0;
    
    // Iterate over free space map extents to allocate blocks
    for (int i = 0; i < vcb->fs_st.extentLength && numBlockReq > 0; i++) {
        
        printf("****** extentLength: %d | numBlockReq: %d | i: %d \
                    *********\n", vcb->fs_st.extentLength, numBlockReq, i);
        

        int startLocation = vcb->free_space_map[i].startLoc;
        int availableBlocks = vcb->free_space_map[i].countBlock; 
            
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
    }
    
    // If unable to fulfill request due to fragmentation, release allocated blocks
    int reqBlockStatus = (numBlockReq > 0) ? -1 : 0; 
    
    // Write updated free space map and VCB to disk; handle write failure
    int writeFSMStatus = ( LBAwrite(vcb->free_space_map, \
            vcb->fs_st.reservedBlocks, 1) < vcb->fs_st.reservedBlocks) ? -1 : 0;
    
    int writeVCBStatus = ( LBAwrite(vcb, 1, 0) < 1 ) ? -1 : 0;


    printf("******ALLOCATE: numBlockReq: %d | writeFSMStatus: %d | writeVCBStatus: %d *********\n", \
                        numBlockReq, writeFSMStatus, writeVCBStatus);
    if (reqBlockStatus == -1 || writeFSMStatus == -1 || writeVCBStatus == -1){
        releaseExtents(requestBlocks);
        return requestBlocks;
    }

    // Reduce the size of memory before return
    requestBlocks.extents = realloc(requestBlocks.extents, requestBlocks.size * sizeof(extent_st) );
    if (requestBlocks.extents == NULL) {
        printf("--------- Fail to realloc memory --------- \n");
    }
    return requestBlocks;
}

/** Releases blocks starting from a specified location by iterates through the 
 * free space map to check if an extent can be merged before adding it to the map. 
 * NOTE: check for overlapping extents (processing... )
 * @return -1 if fail or 0 is sucessed 
 */
int releaseBlocks(int startLoc, int nBlocks) {
    // If the specified range exceeds total blocks, return -1 if error
	int mergeLoc = startLoc + nBlocks;

    if (mergeLoc > vcb->total_blocks) return -1;
    
    int isNotFound = -1;

    // Iterate through the free space map to find a matching extent for merging
    // for (size_t i = 0; i < vcb->fs_st.extentLength; i++) {
        
    //     // Merge if matching extent is found, update location and block count.
    //     if ( mergeLoc == vcb->free_space_map[i].startLoc ) {
    //         printf("==== Merge ========= OK ======\n");
    //         vcb->free_space_map[i].startLoc -= nBlocks;
    //         vcb->free_space_map[i].countBlock += nBlocks;
    //         isNotFound = 0; break;
    //     }
    // }

    // Add a new extent to FS map if no matching merge location is found
    if ( isNotFound ) addExtent(startLoc, nBlocks);

    vcb->fs_st.totalBlocksFree += nBlocks; // Update total free blocks


    // Write updated FS map and VCB to disk, return -1 if writing fails.
    ////////////////////////////////////////////////////////////////////
    if (LBAwrite(vcb->free_space_map, vcb->fs_st.reservedBlocks, \
            vcb->fs_st.curExtentPage) < vcb->fs_st.reservedBlocks) return -1;
    
    //////////////////// CONSIDER WRITE ON CLOSE /////////////////////////////////
    if (LBAwrite(vcb, 1, 0) < 1) return -1;
    printf("====RELEASED===== [%d: %d] =====OK======\n", startLoc, nBlocks);
    return 0;
}

// Adds a new extent to the fs map, specifying starting location and block count.
int addExtent(int startLoc, int countBlock) {

    // If only Primary table exist
    if (vcb->fs_st.extentLength < vcb->fs_st.maxExtent) {
        vcb->free_space_map[vcb->fs_st.extentLength].startLoc = startLoc;
        vcb->free_space_map[vcb->fs_st.extentLength].countBlock = countBlock;
        vcb->fs_st.extentLength++;

        return 0;
    }

    /** Handle cases when we need secondary extent table */
    int innerLength = innerExtentLength(); //2024 % 1024 == 0

    int secondExtTBInx = (vcb->fs_st.extentLength / vcb->fs_st.maxExtent) - 1; // 1024 / 1024 - 1 = 0

    int isPrimaryFull = (vcb->fs_st.extentLength == vcb->fs_st.maxExtent) ? 0 : -1;
    int isSecondFull = ( innerLength == 0 ) ? 0 : -1; // 2024 % 1024 == 0

    if ( (isPrimaryFull == 0) || (isSecondFull == 0) ) {
        /** Create new secondary Extent TB */
        printf("/** Create new secondary Extent TB */\n");
        int status = createSecondaryExtentTB(startLoc, countBlock);
        if (status == -1) return -1;
        printf("/** Create new secondary Extent TB SUCCESS!!! */\n");
    }

    /** Open secondary extent table to add extent:
     * - get 2nd ext table base on the index (secExTBInx)
     * - Load 2nd ext table to memory base on its location loadFreeSpaceMap(###)
     * - Set extent, increse extentLength.
     */

    int secondaryTBLoc = getSecTBLocation(secondExtTBInx);
    
    vcb->free_space_map = loadFreeSpaceMap(secondaryTBLoc);

    vcb->free_space_map[innerLength].startLoc = startLoc;
    vcb->free_space_map[innerLength].countBlock = countBlock;
    vcb->fs_st.extentLength++;
    
    return 0;
    
}

// Create Tertiary Extent and return its location
int createSecondaryExtentTB(int start, int count) {
    /* Create a tersary table:
    Check if 3rd exist:
        If not:
            - Allocate 1 block on disk for the tertiary table
            - Get the location and add [3rd Loc: -3] to the primary extent
            - Allocate another 16 contiguous blocks for the tertiary table
            - Get the location of the secondary extent table
            - Add the 2nd extent table location to the 3rd extent and increase its size
            - Write the 3rd extent to disk (close memory)
            - Load the 2nd extent table into the FS map pointer
            - Add the last extent in Primary to the 2nd table and increase extent len size
            - Reset helper extent to { -1, 0 } Not use it anymore
            - Add new start and count to the 2nd table and increase extent len size
    */
    // This case when tertiary table is not exist.
    extent_st lastPriExt = {-1, 0};
    if (vcb->fs_st.terExtTBLoc == -1) {
        printf(" ----------- createTertiaryExtentTB ----------- \n");
        lastPriExt = createTertiaryExtentTB();
        if ( lastPriExt.startLoc == -1 ) return -1;
    }

    int secondExtLoc = createExtentTables();
    if (secondExtLoc == -1) {
        printf("Error - Can not create secondary extent table\n");
        return -1;
    }
    printf(" ----------- createExtentTables ----------- \n");
    if (!vcb->fs_st.tertiaryExtentTB) {
        vcb->fs_st.tertiaryExtentTB = malloc(vcb->block_size);
    }
    vcb->fs_st.tertiaryExtentTB[vcb->fs_st.tertiaryExtLength++] = secondExtLoc;
    
    int writeStatus = LBAwrite(vcb->fs_st.tertiaryExtentTB, 1, vcb->fs_st.terExtTBLoc);
    if (writeStatus == -1) return -1;

    int innerLength = innerExtentLength();
    
    vcb->free_space_map = loadFreeSpaceMap(secondExtLoc);
    if (vcb->free_space_map == NULL) {
        printf("---- Fail to load FreeSpace Map memory: createSecondaryExtentTB() ----\n");
        return -1;
    }

    if (lastPriExt.startLoc != -1) {
        // Add the last extent in Primary to the 2nd table and increase its size
        printf(" ----------- Add the last extent in Primary to the 2nd table and increase its size ----------- \n");
        vcb->free_space_map[innerLength].startLoc = lastPriExt.startLoc;
        vcb->free_space_map[innerLength].countBlock = lastPriExt.countBlock;
        vcb->fs_st.extentLength++;
    }

    vcb->free_space_map[innerLength].startLoc = start;
    vcb->free_space_map[innerLength].countBlock = count;
    vcb->fs_st.extentLength++;
    // NOTE: Shall I load the pimary extent table after finish???????

    printf(" ----------- createSecondaryExtentTB SECCESS :) ----------- \n");
    return 0;
}

/** Create a tertairy extent table when needed.
 * @return {-1, 0} extent on error or last extent from primary extent table 
 */
extent_st createTertiaryExtentTB() {
    extent_st ext = { -1, 0 };

    extents_st terLoc = allocateBlocks(1, 0);
    if (terLoc.extents == NULL) {
        printf("Error - Can not create tertiary extent\n");
        return ext;
    };

    vcb->fs_st.terExtTBLoc = terLoc.extents[0].startLoc;
    printf("=======createTertiaryExtentTB - 3rd ExtentLoc: [%d: %d] ======\n", \
                                terLoc.extents[0].startLoc, terLoc.extents[0].countBlock);
    releaseExtents(terLoc);

    int preExtIndx = vcb->fs_st.extentLength - 1;

    // Retrieve last extents in Primary Ext Table
    ext = ( extent_st ) { vcb->free_space_map[preExtIndx].startLoc, \
                        vcb->free_space_map[preExtIndx].countBlock };

    printf("======= Last extents in Primary Ext TB [%d: %d] ======\n", \
                        ext.startLoc, ext.countBlock);
    
    printf("======= Extent Length: [%d] ======\n", vcb->fs_st.extentLength);
    vcb->free_space_map[preExtIndx].startLoc = vcb->fs_st.terExtTBLoc;
    vcb->free_space_map[preExtIndx].countBlock = TERTIARY_EXT_COUNT;
    
    printf("======= Extent Length Updated: [%d] ======\n", vcb->fs_st.extentLength);
    
    return ext;
}

/** Allocate blocks for Primary or Secondary Extent and return its start
 * @return location; -1 on error */
int createExtentTables() {
    extents_st allocateExtBlocks = allocateBlocks(vcb->fs_st.reservedBlocks, \
                                                            vcb->fs_st.reservedBlocks);

    if (allocateExtBlocks.extents == NULL || allocateExtBlocks.size != 1) {
        printf("======= Error: Unable to createExtentTables ======\n"); 
        return -1;
    }
    
    int extLoc = allocateExtBlocks.extents[0].startLoc;
    if (allocateExtBlocks.extents) releaseExtents(allocateExtBlocks);
    return extLoc;
}


int getSecTBLocation(int secIdnx) {
    if (!vcb->fs_st.tertiaryExtentTB) {
        vcb->fs_st.tertiaryExtentTB = (int*) allocateMemFS(1);

        int readStatus = LBAread(vcb->fs_st.tertiaryExtentTB, 1, vcb->fs_st.terExtTBLoc);
        if (readStatus < 1) {
            freePtr(vcb->fs_st.tertiaryExtentTB, "Tertiaty TB\n");
            return -1;
        }
    }
    

    int secondaryTBLoc = vcb->fs_st.tertiaryExtentTB[secIdnx];
    printf("****** getSecTBLocation [%d] - SUCCESS :) *******\n", secondaryTBLoc);
    return secondaryTBLoc;
}

int innerExtentLength() {
    return vcb->fs_st.extentLength % vcb->fs_st.maxExtent;
}

// Remove an extent_st from the FreeSpace based on its location
void removeExtent( int startLoc ) {
    for (int i = 0; i < vcb->fs_st.extentLength; i++) {

        if (vcb->free_space_map[i].startLoc == startLoc) {
            // Shift remaining extents to the left
            for (int j = i; j < vcb->fs_st.extentLength - 1; j++) {
                vcb->free_space_map[j] = vcb->free_space_map[j + 1];
            }
            vcb->fs_st.extentLength-- ;
        }
    }
}

/** Reserve the minimum number of blocks required for free space.
 * @return 1 is minimum or number of blocks needed 
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

// Releases memory allocated in allocateBlocks, resets its size
void releaseExtents(extents_st reqBlocks) {
    if (reqBlocks.extents) {
        freePtr(reqBlocks.extents, "blocks allocate");
    } reqBlocks.size = 0;
}

// Frees memory allocated forfree space map and resets the pointer
void freePtr(void* ptr, char* type){
    if (ptr) {
        printf ("Release %s pointer ...\n", type);
        free(ptr);
        ptr = NULL;
    }
}