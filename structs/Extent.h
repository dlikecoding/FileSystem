/**************************************************************
* Class::  CSC-415-03 FALL 2024
* Name:: Danish Nguyen
* Student IDs:: 923091933
* GitHub-Name:: dlikecoding
* Group-Name:: 0xAACD
* Project:: Basic File System
*
* File:: Extent.h
*
* Description:: Structures to manage contiguous blocks of data and 
* the extents_st structure for handling multiple extents in block 
* allocation requests
*
**************************************************************/

#ifndef _EXTENT_H
#define _EXTENT_H


/* Structure representing a single extent of contiguous blocks.
 * - startLoc: The starting location of the contiguous block.
 * - countBlock: The number of contiguous blocks with startLoc included 
 */
typedef struct extent_st {
    int startLoc;
    int countBlock;
} extent_st;

/* Structure for managing multiple extents in block requests.
 * - extents: Pointer to an array of extent_st structures.
 * - size: The total number of extents 
 */ 
typedef struct extents_st {
    extent_st *extents;
    int size;
} extents_st;

#endif