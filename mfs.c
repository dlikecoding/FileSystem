#include "mfs.h"

// Parses a path string to find the directory and the last element name.
int parsePath(const char *path, parsepath_st results) {
    if (path == NULL || strlen(path) == 0) return -1; // Invalid path

    // Start from root or CWD based on absolute or relative path
    directory_entry *start = (path[0] == '/') ? vcb->root_dir_ptr : vcb->cwdLoadDE;
    directory_entry *parent = start;

    char pathCopy[strlen(path)];
    strcpy(pathCopy, path);

    char *token1, *token2, *savePtr;
    token1 = strtok_r(pathCopy, "/", &savePtr);
    
    // If there’s no token, the path is the root directory itself
    if (token1 == NULL) {
        results.parent = parent;
        results.lastElement = NULL;
        results.lastIndex = 0;
        return 0;
    }
    
    // Iterate through tokens to traverse the path
    while (token1 != NULL) {
        token2 = strtok_r(NULL, "/", &savePtr); // Check if there are more tokens
        int idx = findInDir(parent, token1);    // Find the current token in the directory

        if (idx == -1) return -1;  // Path component not found

        if (token2 == NULL) { // Last token in the path
            results.parent = parent;
            results.lastElement = token1;
            results.lastIndex = idx;
            return 0;
        }
        
        if (parent[idx].is_directory != 1) return -1;  // Not a directory

        // Load the next directory level
        directory_entry *newParent = loadDir(&parent[idx]);
        if (newParent == NULL) return -1; // Loading directory failed
        

        // Selectively free the current parent if it's not root or CWD
        freeDirectory(parent);

        parent = newParent;  // Update parent to the newly loaded directory
        token1 = token2; // Move to the next token
    }
    return -1;
}

/** Get current working directory
 * @returns the current working directory as a string in pathname */
char* fs_getcwd(char *pathname, size_t size) {
    strncpy(pathname, vcb->cwdStrPath, size);  //copy CWD string with size limit
    // pathname[size - 1] = '\0';  // Not sure if we need null-termination
    return pathname;
}

int fs_mkdir(const char *pathname, mode_t mode) {
    printf("============== [ MAKE DIR ] =============\n");
    directory_entry *parent;
    int index;
    char *lastElementName;

    int parseStatus = ParsePath(pathname, &parent, &index, &lastElementName);
    if ( parseStatus != 0 || index != -1) {
        printf("============== [ MAKE DIR parseStatus: %d|| index: %d ] =============\n", parseStatus, index);
        return -1;
    }

    directory_entry *newDir = createDirectory(DIRECTORY_ENTRIES, parent);
    if (!newDir) {
        return -1;
    }
    
    int maxNameLen = min(strlen(lastElementName), MAX_FILENAME);

    strncpy(newDir[0].file_name, lastElementName, maxNameLen);
    newDir[0].is_directory = 1;
    newDir[0].is_used = 1;

    if (writeDirHelper(newDir) == -1) {
        freePtr(newDir, "DE msf.c");
        return -1;
    }
    printf("============== [ MAKE DIR: SUCCESS ] =============\n");
    return 0;
}



// Finds an entry in a directory by name. Returns index if found, -1 if not found.
int findInDir(directory_entry *dir, char *name) {
    if (dir == NULL || name == NULL) return -2; // Invalid parameters
    
    // Number of directory entries
    int numEntries = dir[0].file_size / sizeof(directory_entry);
    
    for (int i = 0; i < numEntries; i++) {
        if (dir[i].is_used == 1) {

            // if the name is matched, then return found entry index
            if (strcmp(dir[i].file_name, name) == 0) return i;
        }
    }
    return -1;
}

// Loads a directory from disk based on a directory entry
directory_entry* loadDir(directory_entry *dir) {
    if (dir == NULL || dir->is_directory != 1) return NULL; // Invalid DE

    int blocksNeeded = computeBlockNeeded(dir->file_size, vcb->block_size);
    int bytesNeeded = blocksNeeded * vcb->block_size;
    directory_entry *loadedDE = (directory_entry *)malloc(bytesNeeded);
    if (loadedDE == NULL) return NULL; // Memory allocation failed

    int status = LBAread(&loadedDE, blocksNeeded, dir->extents[0].startLoc);
    if (status < blocksNeeded) {
        printf("Can not load directory");
        return NULL;
    }
    return loadedDE;
}

// Frees a directory, but skips the global root or CWD
void freeDirectory(directory_entry *dir) {
    if (dir == NULL || dir == vcb->root_dir_ptr || dir == vcb->cwdLoadDE) return; // Don't free global directories
    freePtr(dir, "Directory Entry"); // Free the directory entry pointer
}

// Sets the current working directory to the specified `path`.
int fs_setcwd(char *pathname) {
    if (pathname == NULL || strlen(pathname) == 0) return -1;

    char *pathCopy = strdup(pathname);   // Create a modifiable copy of path
    directory_entry *parent;
    int index = -1;
    char *lastElement = NULL;

    // validate the path
    int parseResult = ParsePath(pathCopy, &parent, &index, &lastElement);
    free(pathCopy);  // Clean up path copy after parsing

    if (parseResult != 0 || index == -1) return -1;  // Path invalid or last element not found

    // Check if the final element is a directory
    if (parent[index].is_directory != 1) return -1;

    // Load the new directory
    directory_entry *temp = loadDir(&parent[index]);
    if (temp == NULL) return -1; // Directory loading failed

    // Free the current loadedCWD if it's not root
    if (vcb->cwdLoadDE != vcb->root_dir_ptr) {
        freeDirectory(vcb->cwdLoadDE);
    }

    vcb->cwdLoadDE = temp;  // Update loadedCWD to the new directory

    // Update the cwdString
    char *newCwdString;
    if (pathname[0] == '/') {
        // Absolute path, set cwdString directly
        newCwdString = strdup(pathname);
    } else {
        // Relative path, concatenate with the current cwdString
        size_t newSize = strlen(vcb->cwdStrPath) + strlen(pathname) + 2; // Extra space for "/" and null terminator
        newCwdString = malloc(newSize);
        snprintf(newCwdString, newSize, "%s%s", vcb->cwdStrPath, pathname);
    }

    // Ensure the path ends with a trailing slash
    if (newCwdString[strlen(newCwdString) - 1] != '/') {
        size_t newSize = strlen(newCwdString) + 2;
        newCwdString = realloc(newCwdString, newSize);
        strcat(newCwdString, "/");
    }

    // Collapse /./ and /../ in the newCwdString
    char *collapsedPath = collapsePath(newCwdString);
    free(newCwdString);  // Free temporary path

    // Update cwdString to collapsed path
    if (vcb->cwdStrPath != NULL) free(vcb->cwdStrPath);
    vcb->cwdStrPath = collapsedPath;

    return 0;
}

// Helper function to collapse /./ and /../ sequences in the path
char* collapsePath(char *path) {
    char *tokens[256];    // Array of pointers to tokens (vector table)
    int indexTable[256];   // Index table to help build the final path
    int index = 0;

    // Tokenize the path
    char *savePtr;
    char *token = strtok_r(path, "/", &savePtr);
    int tokenCount = 0;

    while (token != NULL) {
        tokens[tokenCount++] = token;
        token = strtok_r(NULL, "/", &savePtr);
    }

    // Process each token to handle . and ..
    for (int i = 0; i < tokenCount; i++) {
        if (strcmp(tokens[i], ".") == 0) {
            // Ignore single dot
            continue;
        } else if (strcmp(tokens[i], "..") == 0) {
            // Handle double dot by moving back in index unless at the root
            if (index > 0) index--;
        } else {
            // Normal directory entry
            indexTable[index++] = i;
        }
    }

    // Build the collapsed path string
    size_t finalSize = 2; // Starting size for root "/" and null terminator
    for (int i = 0; i < index; i++) {
        finalSize += strlen(tokens[indexTable[i]]) + 1;
    }

    char *collapsedPath = malloc(finalSize);
    if (collapsedPath == NULL) return NULL;

    strcpy(collapsedPath, "/"); // Start with root
    for (int i = 0; i < index; i++) {
        strcat(collapsedPath, tokens[indexTable[i]]);
        strcat(collapsedPath, "/");
    }

    return collapsedPath;
}




///////////////////////////////////////////////////////////////////////////


int fs_setcwd(const char *pathname) {
    directory_entry *parent;
    int index;
    char *lastElementName;
    
    if (ParsePath(pathname, &parent, &index, &lastElementName) != 0 || index == -1) {
        return -1;
    }

    if (parent[index].is_directory != 1) {
        return -1;
    }

    directory_entry *newDir = loadDirectoryEntry(parent[index].data_blocks.extents[0].startLoc);
    if (!newDir) {
        return -1;
    }

    if (loadedCWD != rootDir) releaseDirectory(loadedCWD);
    loadedCWD = newDir;

    free(cwdPath);
    cwdPath = strdup(pathname);
    return 0;
}

int fs_getcwd(char *pathname, size_t size) {
    if (strlen(cwdPath) >= size) return -1;
    strncpy(pathname, cwdPath, size - 1);
    pathname[size - 1] = '\0';
    return 0;
}

int fs_isFile(const char *path) {
    directory_entry *parent;
    int index;
    char *lastElementName;
    
    if (ParsePath(path, &parent, &index, &lastElementName) != 0 || index == -1) return 0;
    return (parent[index].is_directory == 0 && parent[index].is_used);
}

int fs_isDir(const char *path) {
    directory_entry *parent;
    int index;
    char *lastElementName;
    
    if (ParsePath(path, &parent, &index, &lastElementName) != 0 || index == -1) return 0;
    return (parent[index].is_directory == 1 && parent[index].is_used);
}

int fs_mkdir(const char *pathname, mode_t mode) {
    directory_entry *parent;
    int index;
    char *lastElementName;

    if (ParsePath(pathname, &parent, &index, &lastElementName) != 0 || index != -1) {
        return -1;
    }

    directory_entry *newDir = createDirectory(DIRECTORY_ENTRIES, parent);
    if (!newDir) {
        return -1;
    }

    strncpy(newDir[0].file_name, lastElementName, MAX_FILENAME - 1);
    newDir[0].file_name[MAX_FILENAME - 1] = '\0';
    newDir[0].is_directory = 1;
    newDir[0].is_used = 1;

    if (writeDirHelper(newDir) == -1) {
        releaseDirectory(newDir);
        return -1;
    }

    addEntryToParent(parent, newDir);
    return 0;
}

DIR *fs_opendir(const char *path) {
    directory_entry *parent;
    int index;
    char *lastElementName;
    
    if (ParsePath(path, &parent, &index, &lastElementName) != 0 || index == -1) return NULL;
    if (parent[index].is_directory != 1) return NULL;
    
    return (DIR *)loadDirectoryEntry(parent[index].data_blocks.extents[0].startLoc);
}

directory_entry *fs_readdir(DIR *dir) {
    static int readIndex = 0;
    directory_entry *entries = (directory_entry *)dir;

    while (readIndex < DIRECTORY_ENTRIES) {
        if (entries[readIndex].is_used) {
            return &entries[readIndex++];
        }
        readIndex++;
    }

    readIndex = 0; // Reset for next call
    return NULL;
}

int fs_closedir(DIR *dir) {
    releaseDirectory((directory_entry *)dir);
    return 0;
}

int fs_stat(const char *path, struct stat *buf) {
    directory_entry *parent;
    int index;
    char *lastElementName;

    if (ParsePath(path, &parent, &index, &lastElementName) != 0 || index == -1) return -1;

    memset(buf, 0, sizeof(struct stat));
    buf->st_mode = parent[index].is_directory ? S_IFDIR : S_IFREG;
    buf->st_size = parent[index].file_size;
    buf->st_atime = parent[index].access_time;
    buf->st_mtime = parent[index].modification_time;
    buf->st_ctime = parent[index].creation_time;
    return 0;
}

int fs_delete(const char *path) {
    directory_entry *parent;
    int index;
    char *lastElementName;

    if (ParsePath(path, &parent, &index, &lastElementName) != 0 || index == -1) return -1;
    if (parent[index].is_directory) return -1;

    releaseBlocksAndCleanup(parent[index].data_blocks);
    parent[index].is_used = 0;
    return 0;
}

int fs_rmdir(const char *path) {
    directory_entry *parent;
    int index;
    char *lastElementName;

    if (ParsePath(path, &parent, &index, &lastElementName) != 0 || index == -1) return -1;
    if (parent[index].is_directory == 0 || parent[index].file_size > 2 * sizeof(directory_entry)) return -1;

    //Get extents and release all the block.
    // set this dir to unused
    releaseDirectory(&parent[index]);
    parent[index].is_used = 0;
    return 0;
}