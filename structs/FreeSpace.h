#ifndef _FREESPACE_H
#define _FREESPACE_H

#include <stdio.h>
#include <stdlib.h> // malloc
#include <string.h> // memcpy

#include "fsLow.h"
#include "structs/Extent.h"
#include "structs/fs_utils.h"

#define PRIMARY_EXTENT_TB vcb->block_size/sizeof(extent_st)
#define TERTIARY_EXTENT_TB ( PRIMARY_EXTENT_TB * 2 )
#define FRAGMENTATION_PERCENT 0.05 //Number of Blocks need for FreeSpace
#define FREESPACE_START_LOC 1

typedef struct freespace_st {
    unsigned int freeSpaceLoc;    // start location of the free space block on disk.
    unsigned int extentLength;    // number of extents currently in free space map
    unsigned int totalBlocksFree; // total blocks are free on disk
    unsigned int reservedBlocks;  // total number of blocks reserved for extents
    unsigned int maxExtent;       // max extent can store on reserved blocks
    
    /** Last size element in the extent indicates the creation status of secondary 
     * or tertiary extents
     > 0  - indicates presence in the primary extent
     -2   - indicates a secondary extent has been created
     -3   - indicates a tertiary extent has been created */ 
    unsigned int lastExtentSize;
} freespace_st;



int initFreeSpace(int numberOfBlocks, int blockSize); 

int loadFSMap();
void releaseFSMap();

int calBlocksNeededFS(int numberOfBlocks, int blockSize, double fragmentPerc);
void addExtent(int startLoc, int countBlock);
void removeExtent( int startLoc );
int mergeExtent(int startLoc, int countBlock);


void releaseReqBlocks(extents_st reqBlocks);
// int findLBABlockLocation(int n, int nBlock);
// int createSecondaryExtent();
// int createTertiaryExtent();


extents_st allocateBlocks(int nBlocks, int minContinuous);
int releaseBlocks(int startLoc, int nBlocks);



#endif