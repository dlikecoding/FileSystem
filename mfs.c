/**************************************************************
* Class::  CSC-415-03 FALL 2024
* Name:: Danish Nguyen, Atharva Walawalkar, Arvin Ghanizadeh, Cheryl Fong
* Student IDs:: 923091933, 924254653, 922810925, 918157791
* GitHub-Name:: dlikecoding
* Group-Name:: 0xAACD
* Project:: Basic File System
*
* File:: mfs.h
*
* Description:: 
*	This is the file system interface.
*	This is the interface needed by the driver to interact with
*	your filesystem.
*
**************************************************************/

#include "mfs.h"

int fs_mkdir(const char *pathname, mode_t mode) {
    printf(" ******* [ MAKE DIR: START ] ******* \n");
    parsepath_st parser = { NULL, -1, "" };
    int isValid = parsePath(pathname, &parser);

    if (isValid != 0) {
        printf(" ==== [ MAKE DIR parseStatus: %d || index: %d ] ==== \n", isValid, parser.index);
        return -1;
    }
    printf(" *** fs_mkdir: parser.retParent: %d *** \n", isValid, parser.retParent->extents[0].startLoc);
    
    directory_entry *newDir = createDirectory(DIRECTORY_ENTRIES, parser.retParent);
    if (!newDir) return -1;

    // Interate through new dir, find unused dir and create new DE with last element name
    // When done, write this back on the disk
    int numEntries = newDir[0].file_size / sizeof(directory_entry);
    
    time_t curTime = time(NULL);
    
    for (int i = 0; i < numEntries; i++) {
        // Found unused directory entry
        if (!newDir[i].is_used) {
            strncpy(newDir[i].file_name, parser.lastElement, MAX_FILENAME);
            newDir[i].file_size = newDir[0].file_size;
            newDir[i].is_directory = 1;
            newDir[i].is_used = 1;
            newDir[i].creation_time = curTime;
            newDir[i].access_time = curTime;
            newDir[i].modification_time = curTime;
            break;
        }
    }

    if (writeDirHelper(newDir) == -1) {
        freePtr(newDir, "DE msf.c");
        return -1;
    }
    freePtr(newDir, "DE msf.c");
    return 0;
}


// Sets the current working directory to the specified `path`.
int fs_setcwd(char *pathname) {
    parsepath_st parser = { NULL, -1, "" };
    int isValid = parsePath(pathname, &parser);
    printf(" *** fs_setcwd isValid: [%d], parser.index: %d *** \n", isValid, parser.retParent->extents->startLoc);
    if (isValid != 0 || parser.index < 0) {
        printf("is not Valid path\n");
        return -1;
    }

    // If the last element is a directory, return failure
    if ( !parser.retParent[parser.index].is_directory ) {
        printf("is not directory\n");
        return -1;
    }
    
    int loadDELBALoc = parser.retParent[parser.index].extents[0].startLoc;
    directory_entry *newDir = readDirHelper(loadDELBALoc);
    if (!newDir) {
        printf("error - readDirHelper\n");
        return -1;
    }

    freeDirectory(vcb->cwdLoadDE);
    vcb->cwdLoadDE = newDir;

    char newPath[MAX_TOKENS];
    cleanPath(pathname, newPath);
    strcpy(vcb->cwdStrPath, newPath);

    return 0;
}

// Parses a path string to find the directory and the last element name.
int parsePath(const char *path, parsepath_st* parser) {
    if (path == NULL || strlen(path) == 0) return -1; // Invalid path

    // Start from root or CWD based on absolute or relative path
    directory_entry *parent = (path[0] == '/' || !vcb->cwdLoadDE) ? 
                                            vcb->root_dir_ptr : vcb->cwdLoadDE;
    
    if (parent == NULL) return -1; //starting directory error
    
    printf(" *** parent: %d *** \n", parent->file_size);

    char pathCopy[strlen(path)];
    strcpy(pathCopy, path);

    char *token1, *token2, *savePtr;
    token1 = strtok_r(pathCopy, "/", &savePtr);
    
    // If there’s no token, the path is the root directory itself
    if (token1 == NULL) {
        *parser = (parsepath_st) { parent, 0, "" };
        return 0;
    }
    
    // Iterate through tokens to traverse the path
    while (token1 != NULL) {

        token2 = strtok_r(NULL, "/", &savePtr); // Check if there are more tokens
        int idx = findInDir(parent, token1);    // Find the current token in the directory
        printf(" *** while isValid: [%d], idx: %d *** \n", idx, idx);
        if (token2 == NULL) { // Last token in the path
            *parser = (parsepath_st) { parent, idx, "" };
            strcpy(parser->lastElement, token1);
            return 0;
        }
        
        // Path component not found Or Not a directory
        if ((idx == -1) || !parent[idx].is_directory) return -1;

        // Load the next directory level
        directory_entry *newParent = loadDir(&parent[idx]);
        if (newParent == NULL) return -1; // Loading directory failed
        
        // Selectively free the current parent if it's not root or CWD DE
        freeDirectory(parent);

        parent = newParent;  // Update parent to the newly loaded directory
        token1 = token2; // Move to the next token
    }
    
    return -1;
}

/** Finds an entry in a directory by name 
 * @return index if found, -1 if not found
 */ 
int findInDir(directory_entry *de, char *name) {
    printf(" *** findInDir *** \n");
    if (de == NULL || name == NULL) return -2; // Invalid input
    
    // Number of directory entries
    int numEntries = de[0].file_size / sizeof(directory_entry);
    
    for (int i = 0; i < numEntries; i++) {
        if (de[i].is_used) {
            // if the name is matched, then return found entry index
            if (strcmp(de[i].file_name, name) == 0) return i;
        }
    }
    return -1;
}

/** Loads a directory from disk based on parent directory and its index
 * @return directory entry that loaded on disk to memory
 */
directory_entry* loadDir(directory_entry *de) {
    printf(" *** loadDir *** \n");
    if (de == NULL || de->is_directory != 1) return NULL; // Invalid DE
    return readDirHelper(de->extents[0].startLoc);
}

// Frees a directory, but skips the global root or CWD
void freeDirectory(directory_entry *dir) {
    // Don't free global directories
    if (dir == NULL || dir == vcb->root_dir_ptr || dir == vcb->cwdLoadDE) return; 
    freePtr(dir, "Directory Entry"); // Free the directory entry pointer
}


/** Get current working directory
 * @returns the current working directory as a string in pathname 
 */
char* fs_getcwd(char *pathname, size_t size) {
    strncpy(pathname, vcb->cwdStrPath, size);  //copy CWD string with size limit
    return pathname;
}





// Helper function to cleaning up the path for fs_setcwd
void cleanPath(const char* path, char* cleanedPath) {
    char *tokens[MAX_TOKENS];  // Array to store tokens (DE's name)
    int top = 0;               // Index of each DE's name in provied path
    
    // duplicate the path for tokenization
    char pathCopy[strlen(path)];
    strcpy(pathCopy, path);
    
    char *token = strtok(pathCopy, "/"); // Tokenize the path by '/'

    while (token != NULL) {
        if (strcmp(token, ".") == 0) {
            // ignore current directory
        } else if ((strcmp(token, "..") == 0)) {
            // parent directory - move up one level if possible
            if (top > 0) top--;
        } else {
            tokens[top++] = token; // Add valid directory name to the stack
        }
        token = strtok(NULL, "/");
    }

    // Concatenate all tokens into a string
    cleanedPath[0] = '\0';

    // Start with "/" for absolute paths or current working path
    strcat(cleanedPath, (path[0] == '/') ? "/" : vcb->cwdStrPath);
    
    for (int i = 0; i < top; i++) {
        strcat(cleanedPath, tokens[i]);
        strcat(cleanedPath, "/");  // add "/" between directories
    }
}

// ///////////////////////////////////////////////////////////////////////////


// int fs_setcwd(const char *pathname) {
//     directory_entry *parent;
//     int index;
//     char *lastElementName;
    
//     if (parsePath(pathname, &parent, &index, &lastElementName) != 0 || index == -1) {
//         return -1;
//     }

//     if (parent[index].is_directory != 1) {
//         return -1;
//     }

//     directory_entry *newDir = loadDirectoryEntry(parent[index].data_blocks.extents[0].startLoc);
//     if (!newDir) {
//         return -1;
//     }

//     if (loadedCWD != rootDir) releaseDirectory(loadedCWD);
//     loadedCWD = newDir;

//     free(cwdPath);
//     cwdPath = strdup(pathname);
//     return 0;
// }

// int fs_getcwd(char *pathname, size_t size) {
//     if (strlen(cwdPath) >= size) return -1;
//     strncpy(pathname, cwdPath, size - 1);
//     pathname[size - 1] = '\0';
//     return 0;
// }

// int fs_isFile(const char *path) {
//     directory_entry *parent;
//     int index;
//     char *lastElementName;
    
//     if (parsePath(path, &parent, &index, &lastElementName) != 0 || index == -1) return 0;
//     return (parent[index].is_directory == 0 && parent[index].is_used);
// }

// int fs_isDir(const char *path) {
//     directory_entry *parent;
//     int index;
//     char *lastElementName;
    
//     if (parsePath(path, &parent, &index, &lastElementName) != 0 || index == -1) return 0;
//     return (parent[index].is_directory == 1 && parent[index].is_used);
// }

// int fs_mkdir(const char *pathname, mode_t mode) {
//     directory_entry *parent;
//     int index;
//     char *lastElementName;

//     if (parsePath(pathname, &parent, &index, &lastElementName) != 0 || index != -1) {
//         return -1;
//     }

//     directory_entry *newDir = createDirectory(DIRECTORY_ENTRIES, parent);
//     if (!newDir) {
//         return -1;
//     }

//     strncpy(newDir[0].file_name, lastElementName, MAX_FILENAME - 1);
//     newDir[0].file_name[MAX_FILENAME - 1] = '\0';
//     newDir[0].is_directory = 1;
//     newDir[0].is_used = 1;

//     if (writeDirHelper(newDir) == -1) {
//         releaseDirectory(newDir);
//         return -1;
//     }

//     addEntryToParent(parent, newDir);
//     return 0;
// }

// DIR *fs_opendir(const char *path) {
//     directory_entry *parent;
//     int index;
//     char *lastElementName;
    
//     if (parsePath(path, &parent, &index, &lastElementName) != 0 || index == -1) return NULL;
//     if (parent[index].is_directory != 1) return NULL;
    
//     return (DIR *)loadDirectoryEntry(parent[index].data_blocks.extents[0].startLoc);
// }

// directory_entry *fs_readdir(DIR *dir) {
//     static int readIndex = 0;
//     directory_entry *entries = (directory_entry *)dir;

//     while (readIndex < DIRECTORY_ENTRIES) {
//         if (entries[readIndex].is_used) {
//             return &entries[readIndex++];
//         }
//         readIndex++;
//     }

//     readIndex = 0; // Reset for next call
//     return NULL;
// }

// int fs_closedir(DIR *dir) {
//     releaseDirectory((directory_entry *)dir);
//     return 0;
// }

// int fs_stat(const char *path, struct stat *buf) {
//     directory_entry *parent;
//     int index;
//     char *lastElementName;

//     if (parsePath(path, &parent, &index, &lastElementName) != 0 || index == -1) return -1;

//     memset(buf, 0, sizeof(struct stat));
//     buf->st_mode = parent[index].is_directory ? S_IFDIR : S_IFREG;
//     buf->st_size = parent[index].file_size;
//     buf->st_atime = parent[index].access_time;
//     buf->st_mtime = parent[index].modification_time;
//     buf->st_ctime = parent[index].creation_time;
//     return 0;
// }

// int fs_delete(const char *path) {
//     directory_entry *parent;
//     int index;
//     char *lastElementName;

//     if (parsePath(path, &parent, &index, &lastElementName) != 0 || index == -1) return -1;
//     if (parent[index].is_directory) return -1;

//     releaseBlocksAndCleanup(parent[index].data_blocks);
//     parent[index].is_used = 0;
//     return 0;
// }

// int fs_rmdir(const char *path) {
//     directory_entry *parent;
//     int index;
//     char *lastElementName;

//     if (parsePath(path, &parent, &index, &lastElementName) != 0 || index == -1) return -1;
//     if (parent[index].is_directory == 0 || parent[index].file_size > 2 * sizeof(directory_entry)) return -1;

//     //Get extents and release all the block.
//     // set this dir to unused
//     releaseDirectory(&parent[index]);
//     parent[index].is_used = 0;
//     return 0;
// }


int fs_isDir(char *path) {
    parsepath_st parser = { NULL, -1, "" };
    int isValid = parsePath(path, &parser);
    
    if (isValid != 0 || parser.index < 0) return 0;
    return ( parser.retParent[parser.index].is_directory );
}

int fs_isFile(char *path) {
    return ( !fs_isDir(path) );
}
