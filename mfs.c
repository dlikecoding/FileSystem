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

/** Parses a path string to find the directory and the last element name
 * @return 0 on success or -1 on failure
 * @anchor Danish Nguyen
 */
int parsePath(const char *path, parsepath_st* parser) {
    if (path == NULL || strlen(path) == 0) return -1; // Invalid path

    // Start from root or CWD based on absolute or relative path
    directory_entry *parent = (path[0] == '/' || !vcb->cwdLoadDE) ? 
                                            vcb->root_dir_ptr : vcb->cwdLoadDE;
    
    if (parent == NULL) return -1; //starting directory error

    char *pathCopy = strdup(path);

    char *token1, *token2, *savePtr;
    token1 = strtok_r(pathCopy, "/", &savePtr);
    
    // If there’s no token, the path is the root directory itself
    if (token1 == NULL) {
        *parser = (parsepath_st) { parent, 0, "" };
        freePtr( (void**) &pathCopy, "pathCopy");
        return 0;
    }
    
    // Iterate through tokens to traverse the path
    while (token1 != NULL) {

        token2 = strtok_r(NULL, "/", &savePtr); // Check if there are more tokens
        int idx = findInDir(parent, token1);    // Find the current token in the directory
        
        if (token2 == NULL) { // Last token in the path
            *parser = (parsepath_st) { parent, idx, "" };
            strncpy(parser->lastElement, token1, MAX_FILENAME - 1);
            parser->lastElement[MAX_FILENAME - 1] = '\0';
            freePtr( (void**) &pathCopy, "pathCopy");
            return 0;
        }

        // Path component not found Or Not a directory
        if ( (idx == -1) || (!parent[idx].is_directory) ) {
            freePtr( (void**) &pathCopy, "pathCopy");
            return -1;
        }

        // Load the next directory level
        directory_entry *newParent = loadDir(&parent[idx]);
        if (newParent == NULL) {
            freePtr( (void**) &pathCopy, "pathCopy");
            return -1; // Loading directory failed
        } 
        
        // Selectively free the current parent if it's not root or CWD DE
        freeDirectory(parent);

        parent = newParent;  // Update parent to the newly loaded directory
        token1 = token2; // Move to the next token
    }
    freePtr( (void**) &pathCopy, "pathCopy");
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
    freePtr((void**) &dir, "Directory Entry"); // Free the directory entry pointer
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

    if (parsePath(pathname, &parser) != 0) return -1;

    if (parser.index == -1) {
        printf("rm: %s: No such file or directory\n", parser.lastElement );
        return -1; // Can not remove not exist dir
    }

    if (parser.retParent[parser.index].is_directory != isDir) return -1;

    // If target is a directory, loaded to memory and check if it's empty
    if (isDir) { // isDir <=> 1
        directory_entry *removeDir = loadDir(&parser.retParent[parser.index]);
        
        // Ensure DE loaded and is empty before deleting
        if ( !removeDir || !isDirEmpty(removeDir)) {
            printf("Cannot remove '%s': Is a directory and not empty\n", parser.retParent[parser.index].file_name);
            return -1;
        }
        
        // // It's current working DE, can not be deleted
        // if (strcmp(pathname, vcb->cwdStrPath) == 0){
        //     printf("Cannot remove '%s': Current working directory\n", parser.retParent[parser.index].file_name);
        //     return -1;
        // }
        freePtr((void**) &removeDir, "Free rm dir");
    }

    // Mark the target directory/file entry as unused in its parent metadata
    int status = removeDE(parser.retParent, parser.index, 0);

    // Update the parent directory on disk with the changes
    // printf("rmdir - writeDirHelper: loc %d\n", parser.retParent->extents->startLoc);
    // printf("Deleted %s: %s successfuly!\n", (isDir)? "Directory": "File", parser.retParent[parser.index].file_name);
    return (status == -1) ? -1 : writeDirHelper(parser.retParent);
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

    int deIdx = makeDirOrFile(parser, 1, newDir);
    
    // Nomore entry availible in parent directory
    if (deIdx == -1) printf("Error - mkdir: Unable to create directory \n");

    /** Frees memory allocated for the new directory once the operation is complete.
        Writes the changes back to disk after updating. */
    freePtr((void**) &newDir, "DE msf.c");

    // printf(" *** fs_mkdir newDir: [%s] *** \n", parser.retParent[deIdx].file_name);
    // printf(" *** fs_mkdir writeDirHelper: [%d] *** \n", parser.retParent->extents->startLoc);
    
    return (deIdx == -1) ? -1 : writeDirHelper(parser.retParent);
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
    /** Purpose of setcwd is to track user's current DE location
     * Before changing cwd, the following conditions must be met:
     * - The path must exist
     * - The index must not be -1
     * - The des must be a directory
     * 
     * Initially, cwdLoadedDE is set to NULL when user is located in root directory.
     * 
     * When user navigates to a different DE, cwdLoadedDE loads a new DE into memory
     * (root_dir_ptr should never freed)
     * When user navigates back to root DE, previously loaded DE is freed, 
     * and cwdLoadedDE is reset to NULL.
     */

    parsepath_st parser = { NULL, -1, "" };

    if (parsePath(pathname, &parser) != 0 || parser.index == -1) return -1;

    // If the last element exists but is not a directory, return failure
    if ( !parser.retParent[parser.index].is_directory ) return -1;
    
    int newDirLoc = parser.retParent[parser.index].extents->startLoc;

    // If navigating to another directory entry, ensure cwdLoadDE is not NULL and not pointing to root.
    // Free the old cwdLoadDE and load the new directory entry from disk.
    if (vcb->cwdLoadDE && ( vcb->cwdLoadDE != vcb->root_dir_ptr )) {
        freePtr((void**) &vcb->cwdLoadDE, "CWD Str Path");
        vcb->cwdLoadDE = NULL;
    }
    // If cwdLoadDE is root_dir_ptr or NULL, load new DE to cwdLoadDE
    vcb->cwdLoadDE = loadDir(&parser.retParent[parser.index]);
    
    // Create a pointer to point to old cwd string path
    char* oldStrPath = vcb->cwdStrPath;
    
    vcb->cwdStrPath = cleanPath(pathname);
    if (!vcb->cwdStrPath) return -1;

    freePtr((void**) &oldStrPath, "CWD Str Path");
    
    printf(">>> %s\n", vcb->cwdStrPath);
    return 0;
}

/** Sanitized a given path by processing each directory level, resolving any "." 
 * (current directory) or ".." (parent directory) references, and building a clean, 
 * absolute path. Result updates cwd path and stored in vcb->cwdStrPath
 * @anchor Danish Nguyen
 */
char* cleanPath(const char* srcPath) {
    char *tokens[MAX_PATH_LENGTH];  // Array to store tokens (DE's names)
    int curIdx = 0;  // Index of each DE's name in provied path
    
    // duplicate the path for tokenization
    char *pathCopy = (char*) calloc( sizeof(char), MAX_PATH_LENGTH);
    
    // Start with "/" for absolute paths or current working path
    if (srcPath[0] != '/') {
        strcpy(pathCopy, vcb->cwdStrPath);
        strncat(pathCopy, srcPath, (MAX_PATH_LENGTH - strlen(vcb->cwdStrPath) - 1) );
    } else {
        strncpy(pathCopy, srcPath, (MAX_PATH_LENGTH - 1) );
    }
    
    // Reduce memory for path string
    pathCopy = realloc(pathCopy, strlen(pathCopy));

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
    if(newStrPath == NULL) {
        freePtr((void**) &pathCopy, "cleanPath - pathCopy");
        return NULL;
    }
   
    strcpy(newStrPath, "/");

    for (int i = 0; i < curIdx; i++) {
        strcat(newStrPath, tokens[i]);
        strcat(newStrPath, "/");  // add "/" between directories
    }

    freePtr((void**) &pathCopy, "cleanPath - pathCopy");
    return newStrPath;
}

fdDir* fs_opendir(const char *pathname) {
    parsepath_st parser = { NULL, -1, "" };

    if (parsePath(pathname, &parser) != 0 || parser.index < 0) {
        printf("Error: Invalid path - index: %d \n", parser.index);
        return NULL;}
    if (!parser.retParent[parser.index].is_directory) {
        printf("Error: Is not a directory: %d \n", parser.retParent[parser.index].is_directory);
        return NULL;}
    
    fdDir* dirp = malloc(sizeof(fdDir));
    if (dirp == NULL) return NULL;

    dirp->d_reclen = 0;
    dirp->dirEntryPosition = 0;
    
    dirp->de = loadDir(&parser.retParent[parser.index]);
    if (dirp->de == NULL) return NULL;

    dirp->di = malloc(sizeof(struct fs_diriteminfo));
    if (dirp->di == NULL) return NULL;
    printf("Name\t\tSize\t LBA   Used Type Ext  Count\n");
    return dirp;
}

struct fs_diriteminfo* fs_readdir(fdDir* dirp) {
    // printf(" *** fs_diriteminfo: Position: [%d] *** \n", dirp->dirEntryPosition);
    
    if (dirp == NULL || dirp->de == NULL || !dirp->de->is_used) {
        return NULL;
    }    
    // If the last DE has been reached
    if (dirp->dirEntryPosition >= sizeOfDE(dirp->de) - 1) return NULL;

    printf("%s\t\t%-8d   %-5d %3d  %3d  %3d    %-5d\n", 
            dirp->de->file_name,
            dirp->de->file_size,
            dirp->de->extents->startLoc,
            dirp->de->is_used,
            dirp->de->is_directory,
            dirp->de->ext_length,
            dirp->de->extents->countBlock);

    // Populate fs_diriteminfo
    // dirp->di->d_reclen = sizeof(struct fs_diriteminfo);
    // dirp->di->fileType = (dirp->de->is_directory) ? '1': '0';
    // strncpy(dirp->di->d_name, dirp->de->file_name, MAX_FILENAME - 1);
    
    // Move to the next entry in the de
    dirp->de++;
    dirp->dirEntryPosition++;

    return dirp->di;
}

int fs_closedir(fdDir *dirp) {
    if (dirp == NULL) return -1;
    
    // reset posistions
    dirp->de-= dirp->dirEntryPosition;
    dirp->dirEntryPosition = 0;

    freePtr((void**) &dirp->di, "fdDir dir iteminfo");
    freeDirectory(dirp->de);
    freePtr((void**) &dirp, "fdDir");

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

/** Create new file or directory
 * @returns index in parent DE on success, 0 on failure
 * @anchor Danish Nguyen
 */
int makeDirOrFile(parsepath_st parser, int isDir, directory_entry* newDir){
    /** Iterates through the parent directory to find an unused entry.
     * Updates that entry with the last element's name and new DE metadata.*/

    time_t curTime = time(NULL);
    
    for (int i = 0; i < sizeOfDE(parser.retParent); i++) {
        // Found unused directory entry
        if (!parser.retParent[i].is_used) {
            memset(parser.retParent[i].file_name, 0, MAX_FILENAME);
            strncpy(parser.retParent[i].file_name, parser.lastElement, MAX_FILENAME - 1);

            // Create new directory
            if (isDir && newDir) {
                memcpy(parser.retParent[i].extents, newDir->extents, newDir->ext_length * sizeof(extent_st));
                parser.retParent[i].ext_length = newDir->ext_length;
                parser.retParent[i].is_directory = 1;
                parser.retParent[i].file_size = newDir->file_size;
            
            // Create a new file
            } else {
                parser.retParent[i].is_directory = 0;
                parser.retParent[i].file_size = 0;
                parser.retParent[i].ext_length = 0;
            }
            parser.retParent[i].is_used = 1;
            parser.retParent[i].creation_time = curTime;
            parser.retParent[i].access_time = curTime;
            parser.retParent[i].modification_time = curTime;
            return i;
        }
    }
    return -1;
}