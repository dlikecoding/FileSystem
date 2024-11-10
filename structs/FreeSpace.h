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

#ifndef _FREESPACE_H
#define _FREESPACE_H


#include <stdlib.h> // malloc
#include <string.h> // memcpy

#include "fsLow.h"
#include "structs/Extent.h"
#include "structs/fs_utils.h"

#define PRIMARY_EXTENT_TB vcb->block_size/sizeof(extent_st)

#define TERTIARY_EXTENT_TB ( PRIMARY_EXTENT_TB * 2 )
#define TERTIARY_EXT_COUNT -3

#define FRAGMENTATION_PERCENT 0.05 //Number of Blocks need for FreeSpace
#define FREESPACE_START_LOC 1


typedef struct freespace_st {
    unsigned int totalBlocksFree; // total blocks are free on disk
    unsigned int reservedBlocks;  // total number of blocks reserved for extents

    unsigned int extentLength;    // primary extent cursor in free space map
    unsigned int curExtentLBA;    // current start block# of extent table's opening in FS map
    unsigned int maxExtent;       // maximum extents can be store on current extent tables
    
    unsigned int terExtLength;    // tertiary extent length
    int terExtTBLoc;              // tertiary extent start loc

    int* terExtTBMap;      // Retain tertiary Extent TB in memory;
} freespace_st;


extent_st* initFreeSpace(int numberOfBlocks, int blockSize); 
extent_st* loadFreeSpaceMap(int startLoc);

extents_st allocateBlocks(int nBlocks, int minContinuous);
int releaseBlocks(int startLoc, int nBlocks);
void returnExtents(extents_st exts);

int addExtent(int startLoc, int countBlock);
void removeExtent( int startLoc, int i );

// int findLBABlockLocation(int n, int nBlock);



int createExtentTables(int nBlocks, int nContiguous);
int createSecondaryExtentTB();
int createTertiaryExtentTB();
int loadTertiaryTB();

int writeFSToDisk(int startLoc);

int calBlocksNeededFS(int numberOfBlocks, int blockSize);
int getSecTBLocation(int secIdx);

int indexExtentTB();
int secondaryTBIndex();

int isOverlap(extent_st existExt, int addExtStart, int addExtCount);
void pageSwap(int idx, int idxPage);
void freeExtents(extents_st* reqBlocks);
void* allocateMemFS(int nBlocks);

#endif