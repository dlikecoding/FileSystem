/**************************************************************
* Class::  CSC-415-03 FALL 2024
* Name:: Danish Nguyen, Atharva Walawalkar
* Student IDs:: 923091933, 924254653
* GitHub-Name:: dlikecoding
* Group-Name:: 0xAACD
* Project:: Basic File System
*
* File:: ParsePath.h
*
* Description:: ParsePath Structure to hold the result of parsing a path
*
**************************************************************/

#ifndef _PARSEPATH_H_
#define _PARSEPATH_H_

#include "DE.h"

/** ParsePath Structure to hold the result of parsing a path
 * { directory_entry, int, char* } */
typedef struct {
    directory_entry *retParent; // Parent directory entry
    int index;                  // Index of element in the parent
    char lastElement[MAX_FILENAME]; // Name of the last element in the path
} parsepath_st;


#endif