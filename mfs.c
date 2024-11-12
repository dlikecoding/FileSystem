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

void verityDE(directory_entry* de, char* type){
    printf("**** %s ****\n", type);
    if (de) {
        for (size_t i = 0; i < 10; i++){
            printf("Verify: %s ---- isUsed: %d  \n", de[i].file_name, de[i].is_used);
        }
    }
    printf("---------//////------------\n");
}

/** The deleteBlod function deletes a file or directory at a specified path, 
 * ensuring file/directory exists, is the correct type, and is empty if it's a 
 * directory. It then releases any associated storage blocks and updates the 
 * parent directory's metadata.
 * @return 0 on success, -1 on failure
 * @author Danish Nguyen
*/
int deleteBlod(const char* pathname, int isDir) {
    parsepath_st parser = { NULL, -1, "" };

    if (parsePath(pathname, &parser) != 0 || parser.index == -1) return -1; // Can not remove not exist dir
    if (parser.retParent[parser.index].is_directory != isDir) return -1;

    // If target is a directory, loaded to memory and check if it's empty
    if (isDir) { // isDir <=> 1
        directory_entry *removeDir = loadDir(&parser.retParent[parser.index]);
        
        // Ensure it can be loaded and is empty before deleting
        if ( !removeDir || !isDirEmpty(removeDir)) {
            printf("Cannot remove '%s': Is a directory and not empty\n", parser.retParent[parser.index].file_name);
            return -1;
        }
        freePtr(removeDir, "Free rm dir");
    }

    // Mark the target directory/file entry as unused in its parent metadata
    parser.retParent[parser.index].is_used = 0;
    int status = releaseDeExtents(parser.retParent, parser.index);
    if (status == -1) return -1;

    // Update the parent directory on disk with the changes
    if ( writeDirHelper(parser.retParent) == -1) return -1;
    printf("rmdir - writeDirHelper: loc %d\n", parser.retParent->extents->startLoc);
    
    printf("Deleted %s: %s successfuly!\n", (isDir)? "Directory": "File", parser.retParent[parser.index].file_name);
    return 0;
}

/** Deletes a file at a specified path
 * @return 0 on success, -1 on failure
 * @author Danish Nguyen
*/
int fs_delete(const char* filename) {
    return deleteBlod(filename, 0);
}

/** Deletes a directory at a specified path, 
 * @return 0 on success, -1 on failure
 * @author Danish Nguyen
*/
int fs_rmdir(const char *pathname) {
    return deleteBlod(pathname, 1);
}

/** Iterate through each entry in the directory and count entries that are 
 * marked as "is_used"
 * @return true (non-zero) if the directory contains only "." and ".." entries.
 * @author Danish Nguyen
*/
int isDirEmpty(directory_entry *de) {
    int idx = 0;
    for (size_t i = 0; i < sizeOfDE(de); i++) {
        if (de[i].is_used) idx++;
    }
    return idx < 3;
}

/** Creates a new directory at the specified pathname with the given mode 
 * Validates the path, allocates a directory entry, updates directory metadata, and writes 
 * it back to disk.
 * @return 0 on success, -1 on failure
 * @author Danish Nguyen
 */
int fs_mkdir(const char *pathname, mode_t mode) {
    parsepath_st parser = { NULL, -1, "" };

    /** The path must be valid, and last index must be -1
     * indicating that there is no existing entry with the same name */
    if (parsePath(pathname, &parser) != 0) return -1;
    if ( parser.index != -1 ) {
        printf("Error - mkdir: \"%s\": File exists \n", parser.retParent[parser.index].file_name);
        return -1;
    }
    
    directory_entry *newDir = createDirectory(DIRECTORY_ENTRIES, parser.retParent);
    if (!newDir) return -1;

    /** Iterates through the parent directory to find an unused entry.
     * Updates that entry with the last element's name and new DE metadata.
     * Frees memory allocated for the new directory once the operation is complete.
     * Writes the changes back to disk after updating.
     */
    time_t curTime = time(NULL);
    
    for (int i = 0; i < sizeOfDE(newDir); i++) {
        // Found unused directory entry
        if (!parser.retParent[i].is_used) {
            strncpy(parser.retParent[i].file_name, parser.lastElement, MAX_FILENAME);

            memcpy(parser.retParent[i].extents, newDir->extents, newDir->ext_length * sizeof(extent_st));
            parser.retParent[i].ext_length = newDir->ext_length;

            parser.retParent[i].file_size = newDir->file_size;
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
    printf(" *** fs_mkdir writeDirHelper: [%d] *** \n", parser.retParent->extents->startLoc);
    
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

    char pathCopy[MAX_PATH_LENGTH];
    strncpy(pathCopy, path, MAX_PATH_LENGTH);

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

/** Changes the current working directory to the new directory
 * Updates the current working directory, including the current load DE and cwd string path.
 * @return 0 on success, -1 on failure
 * @author Danish Nguyen
*/
int fs_setcwd(char *pathname) {
    /** Main part of setcwd is track where user is locate in which DE
     * Before changed cwd, the path must exist, index is not -1, and destination must be directory
     * Initial, cwdLoadedDE is NULL when user located in root dir
     * when user navigate to other DE, cwd will load new DE to memory (do not free root dir)
     * when user navigate back to the root DE, then free previous DE loaded in cwd, set it to null.
    */
    parsepath_st parser = { NULL, -1, "" };

    if (parsePath(pathname, &parser) != 0 || parser.index == -1) return -1;

    // If the last element exists but is not a directory, return failure
    if ( !parser.retParent[parser.index].is_directory ) return -1;
    
    int newDirLoc = parser.retParent[parser.index].extents->startLoc;

    if (vcb->cwdLoadDE && vcb->cwdLoadDE->extents->startLoc == newDirLoc) {
        printf("************ Current working *************\n");
    } else if (vcb->root_dir_ptr->extents->startLoc == newDirLoc) {
        printf("************ Root DE *************\n");
    } else {
        printf("************ NEW DE *************\n");
    }
    

    
    ////////////////////////////////////////////////////////////////////////
    // if user navigate to root, the newDirLoc is also the root start loc,
    // then free the cwdLoadDE and set it to null
    if ( newDirLoc == vcb->root_dir_ptr->extents->startLoc ) {
        if (vcb->cwdLoadDE) { 
            freePtr(vcb->cwdLoadDE, "free Loaded DE");
            vcb->cwdLoadDE = NULL;
        }
    } else {
        // if user navigate to other de, make sure the cwd is not null and not root
        // Then free the old location and loads the directory entry from disk 
        // to cwdLoadDE.
        if (vcb->cwdLoadDE && vcb->cwdLoadDE != vcb->root_dir_ptr) freePtr(&vcb->cwdLoadDE, "Free CWD");
        vcb->cwdLoadDE = loadDir(&parser.retParent[parser.index]);
        if (!vcb->cwdLoadDE) return -1;   
    }
    //////////////////////////////////////////////////////////////////////

    // Create a pointer to point to old cwd string path
    char* oldStrPath = vcb->cwdStrPath;
    
    vcb->cwdStrPath = cleanPath(pathname);
    if (!vcb->cwdStrPath) return -1;

    freePtr(oldStrPath, "CWD Str Path");

    // if ( writeDirHelper(parser.retParent) == -1) return -1;
    printf("writeDirHelper: loc %d\n", parser.retParent->extents->startLoc);

    printf(">>> %s\n", vcb->cwdStrPath);
    
    return 0;
}


/** Normalizes a given file path by processing each directory level, resolving any "." 
 * (current directory) or ".." (parent directory) references, and building a clean, 
 * absolute path. Result updates the current working directory path, stored in vcb->cwdStrPath
 * @anchor Danish Nguyen
 */
char* cleanPath(const char* srcPath) {
    char *tokens[MAX_PATH_LENGTH];  // Array to store tokens (DE's names)
    int curIdx = 0;  // Index of each DE's name in provied path
    
    // duplicate the path for tokenization
    char pathCopy[strlen(srcPath)];
    
    // Start with "/" for absolute paths or current working path
    if (srcPath[0] != '/') {
        strcpy(pathCopy, vcb->cwdStrPath);
        strcat(pathCopy, srcPath);
    } else {
        strcpy(pathCopy, srcPath);
    }
    
    char *savePtr;
    char *token = strtok_r(pathCopy, "/", &savePtr); // Tokenize the path by '/'
    
    while (token != NULL) {
        if (strcmp(token, ".") == 0) { // ignore current directory

        } else if ((strcmp(token, "..") == 0)) {
            // parent - move up one level if possible
            if (curIdx > 0) curIdx--;
        
        } else {
            tokens[curIdx++] = token; // Add valid directory name to the stack
        }
        token = strtok_r(NULL, "/", &savePtr);
    }
    
    // Concatenate all tokens into a string
    char *newStrPath = malloc(MAX_PATH_LENGTH);
    if(newStrPath == NULL) return NULL;
   
    strcpy(newStrPath, "/");

    for (int i = 0; i < curIdx; i++) {
        strcat(newStrPath, tokens[i]);
        strcat(newStrPath, "/");  // add "/" between directories
    }
    return newStrPath;
}

/////////////////////////////////////////////////////////////////////////////

fdDir* fs_opendir(const char *pathname) {
    parsepath_st parser = { NULL, -1, "" };
    int isValid = parsePath(pathname, &parser); 
    if (isValid != 0 || parser.index < 0) {
        printf("Error: Invalid: %d - index: %d \n", isValid, parser.index);
        return NULL;}
    if (!parser.retParent[parser.index].is_directory) {
        printf("Error: Invalid - is Not Dir: %d \n", parser.retParent[parser.index].is_directory);
        return NULL;}
    
    
    fdDir* dirp = malloc(sizeof(fdDir));
    if (dirp == NULL) return NULL;

    ////////////////////////////////////////////////
    dirp->d_reclen = 0;
    dirp->dirEntryPosition = 0;
    
    dirp->directory = loadDir(&parser.retParent[parser.index]);
    if (dirp->directory == NULL) return NULL;

    dirp->di = malloc(sizeof(struct fs_diriteminfo));
    if (dirp->di == NULL) return NULL;
    
    return dirp;
}

struct fs_diriteminfo* fs_readdir(fdDir* dirp) {
    // printf(" *** fs_diriteminfo: Position: [%d] *** \n", dirp->dirEntryPosition);
    
    if (dirp == NULL || dirp->directory == NULL || !dirp->directory->is_used) {
        return NULL;
    }

    // If the last DE has been reached
    if (dirp->dirEntryPosition >= sizeOfDE(dirp->directory) - 1) return NULL;
    printf(" name: %s\t%d\t%d\t%d\t%ld\t%d\n", 
            dirp->directory->file_name, dirp->directory->file_size,
            dirp->directory->extents->startLoc, dirp->directory->extents->countBlock,
            dirp->directory->creation_time, dirp->directory->is_used);

    // Populate fs_diriteminfo
    // dirp->di->d_reclen = sizeof(struct fs_diriteminfo);
    // dirp->di->fileType = (dirp->directory->is_directory) ? '1': '0';
    // strcpy(dirp->di->d_name, dirp->directory->file_name);
    
    // Move to the next entry in the directory
    dirp->directory++;
    dirp->dirEntryPosition++;

    return dirp->di;
}

int fs_closedir(fdDir *dirp) {
    if (dirp == NULL) return -1;
    
    // reset posistions
    dirp->directory-= dirp->dirEntryPosition;
    dirp->dirEntryPosition = 0;

    freePtr(dirp->di, "fdDir dir iteminfo");
    freeDirectory(dirp->directory);
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


/** Checks if a given path corresponds to a directory
 * @returns 0 if is directory, -1 if not
 * @anchor Danish Nguyen
 */
int fs_isDir(char *path) {
    parsepath_st parser = { NULL, -1, "" };
    int isValid = parsePath(path, &parser);
    
    if (isValid != 0 || parser.index < 0) return 0;
    return ( parser.retParent[parser.index].is_directory );
}

/** Checks if a given path corresponds to a directory
 * @returns 0 if is file, -1 if not
 * @anchor Danish Nguyen
 */
int fs_isFile(char *path) {
    return ( !fs_isDir(path) );
}
