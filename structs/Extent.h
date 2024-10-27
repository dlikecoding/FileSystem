#ifndef _EXTENT_H
#define _EXTENT_H

typedef struct extent_st {
    int startLoc;   // starting location of a contiguous block
    int countBlock; // how many contiguous blocks exist
} extent_st;

// Pointer used in allocated number of block requests 
typedef struct extents_st {
    extent_st *extents; // pointer to extent
    int size;           // total extents
} extents_st;

#endif