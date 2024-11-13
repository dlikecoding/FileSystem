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

#ifndef _FS_UTILS_C
#define _FS_UTILS_C

#include <stdlib.h>
#include <stdio.h>

int computeBlockNeeded(int m, int n);

int min(int a, int b);

void freePtr(void** ptr, const char* type);



#endif

