#ifndef _DE_H
#define _DE_H

#include <time.h>

#include "structs/VCB.h"
#include "structs/FreeSpace.h"
#include "structs/Extent.h"
#include "structs/fs_utils.h"

#define BLOCK_SIZE vcb->block_size        
#define UNUSED_ENTRY_MARKER '\0' // Marker for unused entries

typedef struct {
    time_t creation_time;               // creation time of the file or directory
    time_t modification_time;           // last modification time
    time_t access_time;                 // last access time
    
    int file_size;                      // size of the file or directory
    int is_directory;                   // 1 if directory, 0 if file
    int is_used;                        // 1 if used, 0 if unused
    
    char file_name[MAX_FILENAME];       // File or directory name
    
    extent_st block_location; // Array of extents for data blocks
} directory_entry;

directory_entry *create_directory(int numEntries, directory_entry *parent);

void createDirHelper(directory_entry de, char *fName, 
    extent_st loc, int fSize, int isDir, int isUsed, time_t ctime,
    time_t mTime, time_t aTime);

int writeDirHelper(directory_entry *newDir);

#endif