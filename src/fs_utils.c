/**************************************************************
* Class::  CSC-415-03 FALL 2024
* Name:: Danish Nguyen
* Student IDs:: 923091933
* GitHub-Name:: dlikecoding
* Group-Name:: 0xAACD
* Project:: Basic File System
*
* File:: fs_utils.h
*
* Description:: External utility functions that may be required for 
* operations within the file system project
*
**************************************************************/

#include "structs/fs_utils.h"

/** Computes the number of blocks needed to store a given amount of data.
 * @param m The size of the data in bytes.
 * @param n The size of a single block in bytes.
 * @return The number of blocks needed 
 */
int computeBlockNeeded(int m, int n) {
    return (m + (n - 1)) /n;
}

int min(int a, int b) {
    return (a) < (b) ? (a) : (b);
} 

// Frees memory allocated forfree space map and resets the pointer
void freePtr(void** ptr, const char* type){
    if (ptr && *ptr) {
        // printf ("Release %s pointer ...\n", type);
        free(*ptr);
        *ptr = NULL;
    }
}