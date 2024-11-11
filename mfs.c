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

// Number of directory entries
int sizeOfDE (directory_entry* de) {
    return de[0].file_size / sizeof(directory_entry);
}

int deleteBlod(const char* pathname, int isDir) {
    parsepath_st parser = { NULL, -1, "" };
    int isValid = parsePath(pathname, &parser);

    if (isValid != 0 || parser.index == -1) return -1; // Can not remove not exist dir
    if (parser.retParent[parser.index].is_directory != isDir) return -1;

    int idx = findInDir(parser.retParent, parser.lastElement);

    if (isDir) { // isDir <==> 1
        directory_entry *removeDir = loadDir(&parser.retParent[idx]);
        if (!removeDir) return -1;
        printf(" *** removeDir : [%d, %d] *** \n", removeDir->extents->startLoc, removeDir->extents->countBlock);
        
        if (!isDirEmpty(removeDir)) {
            printf("Directory is not empty \n");
            return -1;
        }
        freePtr(removeDir, "Free rm dir");
    }

    parser.retParent += parser.index;
    // set this metadata in parent to unused, release all blocks associate with it to freespace.
    parser.retParent->is_used = 0;
    for (size_t i = 0; i < parser.retParent->ext_length; i++) {
        int start = parser.retParent->extents[i].startLoc;
        int count = parser.retParent->extents[i].countBlock;
        releaseBlocks(start, count);
    }
    
    // parser.retParent -= parser.index;
    // if ( writeDirHelper(parser.retParent) == -1) return -1;
    
    printf(" *** Write to disk : [%d, %d] *** \n", parser.retParent[0].extents->startLoc, parser.retParent[0].extents->countBlock);
    printf(" ----- Delete %s successfuly! ----- \n", (isDir)? "Directory": "File");
    return 0;
}

int fs_delete(const char* filename) {
    deleteBlod(filename, 0);
    return 0;
}


int fs_rmdir(const char *pathname) {
    deleteBlod(pathname, 1);
    // parsepath_st parser = { NULL, -1, "" };
    // int isValid = parsePath(pathname, &parser);

    // if (isValid != 0 || parser.index == -1) return -1; // Can not remove not exist dir
    // if (!parser.retParent[parser.index].is_directory) return -1;

    // int idx = findInDir(parser.retParent, parser.lastElement);
    // directory_entry *removeDir = loadDir(&parser.retParent[idx]);
    // if (!removeDir) return -1;

    // printf(" ******* [ RM Parent: %s ] ******* \n", parser.retParent->file_name);
    // printf(" ******* [ RM DIR: %s ] ******* \n", parser.retParent[parser.index].file_name);

    // printf(" ******* [ RM Parent: %d ] ******* \n", parser.retParent->extents->startLoc);
    // printf(" ******* [ RM DIR: %d ] ******* \n", removeDir->extents->startLoc);
    
    // printf(" ******* [ RM DIR Start: %d ] ******* \n", parser.retParent[parser.index].extents->startLoc);
    // printf(" ******* [ RM DIR count: %d ] ******* \n", parser.retParent[parser.index].extents->countBlock);
    
    
    // int isEmpty = isDirEmpty(removeDir);
    // if (isEmpty) {
    //     // set this dir in parent to unused, release all blocks in remove dir to freespace.
    //     parser.retParent[parser.index].is_used = 0;
    //     for (size_t i = 0; i < removeDir->ext_length; i++) {
    //         int start = removeDir->extents[i].startLoc;
    //         int count = removeDir->extents[i].countBlock;
    //         releaseBlocks(start, count);
    //     }
    //     printf("Removed dir \n");
    // } else {
    //     printf("CAN NOT remove dir \n");
    // }

    // freePtr(removeDir, "Free rm dir");
    return 0;
}

int isDirEmpty(directory_entry *de) {
    int idx = 0;
    for (size_t i = 0; i < sizeOfDE(de); i++) {
        if (de[i].is_used) idx++;
    }
    return idx < 3; // Only contain "." and ".."
}

/** Creates a new directory at the specified pathname with the given mode 
 * Validates the path, allocates a directory entry, updates directory metadata, and writes 
 * it back to disk.
 * @return 0 on success, -1 on failure
 * @author Danish Nguyen
 */
int fs_mkdir(const char *pathname, mode_t mode) {
    parsepath_st parser = { NULL, -1, "" };
    int isValid = parsePath(pathname, &parser);

    if (isValid != 0) {
        printf(" ==== Error fs_mkdir - status: %d || index: %d ==== \n", isValid, parser.index);
        return -1;
    }
    
    directory_entry *newDir = createDirectory(DIRECTORY_ENTRIES, parser.retParent);
    if (!newDir) return -1;

    // Iterates through the parent directory, finds an unused entry, and update it fields
    // with the last element's name and new DE metadata.
    // After updating, writes the changes back to disk.
    int numEntries = newDir[0].file_size / sizeof(directory_entry);
    
    time_t curTime = time(NULL);
    
    for (int i = 0; i < numEntries; i++) {
        // Found unused directory entry
        if (!parser.retParent[i].is_used) {
            strncpy(parser.retParent[i].file_name, parser.lastElement, MAX_FILENAME);

            memcpy(parser.retParent[i].extents, newDir[0].extents, newDir[0].ext_length * sizeof(extent_st));
            parser.retParent[i].ext_length = newDir->ext_length;

            parser.retParent[i].file_size = newDir[0].file_size;
            parser.retParent[i].is_directory = 1;
            parser.retParent[i].is_used = 1;

            parser.retParent[i].creation_time = curTime;
            parser.retParent[i].access_time = curTime;
            parser.retParent[i].modification_time = curTime;
            break;
        }
    }

    freePtr(newDir, "DE msf.c");

    if (writeDirHelper(parser.retParent) == -1) return -1;
    printf(" *** fs_mkdir vcb->cwdStrPath: [%s] *** \n", vcb->cwdStrPath);
    
    return 0;
}

/** Changes the current working directory to pathname if valid and a directory
 * Updates cwd DE and cwd path. Write old parent to disk.
 * @return 0 on success, -1 on failure
 * @author Danish Nguyen
*/
int fs_setcwd(char *pathname) {
    parsepath_st parser = { NULL, -1, "" };
    int isValid = parsePath(pathname, &parser);
    
    if (isValid != 0 || parser.index < 0) {
        printf(" ---- Invalid Path ---- \n");
        return -1;
    }

    // If the last element is a directory, return failure
    if ( !parser.retParent[parser.index].is_directory ) {
        printf(" ---- Is not a directory ---- \n");
        return -1;
    }
    
    int loadDELBALoc = parser.retParent[parser.index].extents->startLoc;
    
    directory_entry *newDir = readDirHelper(loadDELBALoc);
    if (!newDir) return -1;

    freeDirectory(vcb->cwdLoadDE);
    vcb->cwdLoadDE = newDir;

    cleanPath(pathname);
    
    // int numEntries = parser.retParent[0].file_size / sizeof(directory_entry);
    // for (size_t i = 0; i < numEntries; i++) {
    //     if (parser.retParent[i].is_used) {
    //         printf("parent [%ld]: name[%s] | isUsed: %d, start: %d, count: %d \n", 
    //         i, parser.retParent[i].file_name, 
    //         parser.retParent[i].is_used, 
    //         parser.retParent[i].extents->startLoc, 
    //         parser.retParent[i].extents->countBlock);
    //     }
    // }

    // if ( writeDirHelper(parser.retParent) == -1) return -1;
    printf(">>> %s\n", vcb->cwdStrPath);
    
    return 0;
}

/** Parses a path string to find the directory and the last element name
 * @return int status, a pointer to parsepath_st
 * @anchor Danish Nguyen
 */
int parsePath(const char *path, parsepath_st* parser) {
    if (path == NULL || strlen(path) == 0) return -1; // Invalid path

    // Start from root or CWD based on absolute or relative path
    directory_entry *parent = (path[0] == '/' || !vcb->cwdLoadDE) ? 
                                            vcb->root_dir_ptr : vcb->cwdLoadDE;
    
    if (parent == NULL) return -1; //starting directory error

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
        
        if (token2 == NULL) { // Last token in the path
            *parser = (parsepath_st) { parent, idx, "" };
            strcpy(parser->lastElement, token1);
            return 0;
        }
        
        // Path component not found Or Not a directory
        if ( (idx == -1) || (!parent[idx].is_directory) ) return -1;

        // Load the next directory level
        directory_entry *newParent = loadDir(&parent[idx]);
        if (newParent == NULL) return -1; // Loading directory failed
        
        printf(" *** ParsePath parent: [%s], newParent: %s *** \n", 
                                    parent->file_name, newParent->file_name);
        // Selectively free the current parent if it's not root or CWD DE
        freeDirectory(parent);

        parent = newParent;  // Update parent to the newly loaded directory
        token1 = token2; // Move to the next token
    }
    
    return -1;
}

/** Finds an entry in a directory by name 
 * @return index if found, -1 if not found
 * @anchor Danish Nguyen
 */ 
int findInDir(directory_entry *de, char *name) {
    if (de == NULL || name == NULL) return -2; // Invalid input
    
    for (int i = 0; i < sizeOfDE(de); i++) {
        if (de[i].is_used) {
            // if the name is matched, then return found entry index
            if (strcmp(de[i].file_name, name) == 0) return i;
        }
    }
    return -1;
}

/** Loads a directory from disk based on parent directory and its index
 * @return directory entry that loaded on disk to memory
 * @anchor Danish Nguyen
 */
directory_entry* loadDir(directory_entry *de) {
    if (de == NULL || de->is_directory != 1) return NULL; // Invalid DE
    return readDirHelper(de->extents[0].startLoc);
}

/** Frees a directory entry but skip the root or current working directory.
 * @anchor Danish Nguyen
 */
void freeDirectory(directory_entry *dir) {
    // Don't free global directories
    if (dir == NULL || dir == vcb->root_dir_ptr || dir == vcb->cwdLoadDE) return; 
    freePtr(dir, "Directory Entry"); // Free the directory entry pointer
}


/** Get current working directory
 * @returns the current working directory as a string in pathname
 * @anchor Danish Nguyen
 */
char* fs_getcwd(char *pathname, size_t size) {
    strncpy(pathname, vcb->cwdStrPath, size);  //copy CWD string with size limit
    return pathname;
}

/** Cleans up a file path, resolving "." and "..", and updates
 * current working directory path with the simplified path
 * @anchor Danish Nguyen
 */  
void cleanPath(const char* srcPath) {
    char *tokens[MAX_PATH_LENGTH];  // Array to store tokens (DE's names)
    int top = 0;               // Index of each DE's name in provied path
    
    // duplicate the path for tokenization
    char pathCopy[strlen(srcPath)];
    strcpy(pathCopy, srcPath);
    
    char *savePtr;
    char *token = strtok_r(pathCopy, "/", &savePtr); // Tokenize the path by '/'
    
    while (token != NULL) {
        if (strcmp(token, ".") == 0) {
            // ignore current directory
        } else if ((strcmp(token, "..") == 0)) {
            // parent directory - move up one level if possible
            if (top > 0) top--;
        } else {
            tokens[top++] = token; // Add valid directory name to the stack
        }
        token = strtok_r(NULL, "/", &savePtr);
    }
    
    // Concatenate all tokens into a string
    char newString[MAX_PATH_LENGTH] = "";

    // Start with "/" for absolute paths or current working path
    strcpy(newString, (srcPath[0] == '/') ? "/" : vcb->cwdStrPath);

    for (int i = 0; i < top; i++) {
        strcat(newString, tokens[i]);
        strcat(newString, "/");  // add "/" between directories
    }
    strcpy(vcb->cwdStrPath, newString);
}

// ///////////////////////////////////////////////////////////////////////////


fdDir* fs_opendir(const char *pathname) {
    printf(" *** fs_opendir: *** \n");
    parsepath_st parser = { NULL, -1, "" };
    int isValid = parsePath(pathname, &parser);
    
    if (isValid != 0 || parser.index < 0) return NULL;
    if (!parser.retParent[parser.index].is_directory) return NULL;
    
    
    fdDir* dirp = malloc(sizeof(fdDir));
    if (dirp == NULL) return NULL;

    dirp->d_reclen = sizeof(struct fs_diriteminfo);
    dirp->dirEntryPosition = 0;
    
    dirp->originalDE = loadDir(&parser.retParent[parser.index]);
    dirp->directory = dirp->originalDE;
    if (dirp->directory == NULL) {
        freePtr(dirp, "fdDir");
        return NULL;
    }

    dirp->di = malloc(sizeof(struct fs_diriteminfo));
    if (dirp->di == NULL) {
        freePtr(dirp->directory, "fdDir DE");
        freePtr(dirp, "fdDir");
        return NULL;
    }
    
    return dirp;
}

struct fs_diriteminfo* fs_readdir(fdDir* dirp) {
    // printf(" *** fs_diriteminfo: Position: [%d] *** \n", dirp->dirEntryPosition);
    
    if (dirp == NULL || dirp->directory == NULL) {
        return NULL;
    }

    // If the last DE has been reached
    if (dirp->dirEntryPosition >= sizeOfDE(dirp->directory) - 1) return NULL;
    printf(" |\tname: [%s]\tsize: %d \n", 
            dirp->directory->file_name, dirp->directory->file_size);

    // Populate fs_diriteminfo
    dirp->di->d_reclen = sizeof(struct fs_diriteminfo);
    dirp->di->fileType = (dirp->directory->is_directory) ? '1': 0;
    strcpy(dirp->di->d_name, dirp->directory->file_name);
    
    // Move to the next entry in the directory
    dirp->directory++;
    dirp->dirEntryPosition++;

    return dirp->di;
}

int fs_closedir(fdDir *dirp) {
    printf(" *** fs_closedir: *** \n");
    if (dirp == NULL) return -1;
    
    freePtr(dirp->di, "fdDir dir iteminfo");
    freePtr(dirp->originalDE, "fdDir DE origin");
    freePtr(dirp, "fdDir");
    return 0;
}

int fs_stat(const char *path, struct fs_stat *buf) {
    parsepath_st parser = { NULL, -1, "" };
    int isValid = parsePath(path, &parser);
    
    if (isValid != 0 || parser.index < 0) return -1;
    memset(buf, 0, sizeof(fs_stat));

    int countBlocks = 0;
    for (size_t i = 0; i < parser.retParent->ext_length; i++) {
        countBlocks += parser.retParent[parser.index].extents[i].countBlock;
    }

    buf->st_size = parser.retParent[parser.index].file_size;
    buf->st_blocks = countBlocks;
    buf->st_accesstime = parser.retParent[parser.index].access_time;
    buf->st_modtime = parser.retParent[parser.index].modification_time;
    buf->st_createtime = parser.retParent[parser.index].creation_time;
    return 0;
}



int fs_isDir(char *path) {
    parsepath_st parser = { NULL, -1, "" };
    int isValid = parsePath(path, &parser);
    
    if (isValid != 0 || parser.index < 0) return 0;
    return ( parser.retParent[parser.index].is_directory );
}

int fs_isFile(char *path) {
    return ( !fs_isDir(path) );
}
