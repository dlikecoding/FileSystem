#ifndef _PARSEPATH_H_
#define _PARSEPATH_H_

#include "DE.h"

//Structure to hold the result of parsing a path.
typedef struct {
    directory_entry *parent;    // Parent directory entry
    int lastIndex;           // Index of the last element in the parent
    char *lastElement;        // Name of the last element in the path
} parsepath_st;


#endif