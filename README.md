Here's a concise and structured README.md for your filesystem project:

---

# Basic File System Project (CSC415)

A multi-phase group assignment in C focused on designing and implementing a functional filesystem on a virtual disk using Ubuntu Linux.

## Project Objectives

* Design and implement filesystem components.
* Understand low-level file operations.
* Manage directory structures and metadata.
* Implement advanced buffering and persistence.
* Efficiently allocate and manage free space.

## Phases

The project consists of three phases:

### Phase 1: Volume Formatting

* Format and initialize the volume.
* Set up free space management structures.
* Define initial volume parameters.

### Phase 2: Directory Operations

* Implement directory management (create, delete, list, navigate).
* Maintain and track directory metadata.

### Phase 3: File Operations

* Support file creation, reading, writing, deletion, and metadata handling.
* Integrate file tracking with directory and free space management.

## Provided Components

* Low-level block read/write functions (`fsLow.o` and `fsLow.h`):

  ```c
  uint64_t LBAwrite(void *buffer, uint64_t lbaCount, uint64_t lbaPosition);
  uint64_t LBAread(void *buffer, uint64_t lbaCount, uint64_t lbaPosition);
  ```
* Hexdump utility (`Hexdump/`) to analyze volume content.
* Sample filesystem shell (`fsshell.c`) for interaction and testing.

## Required Interfaces

### File Operations

```c
b_io_fd b_open(char *filename, int flags);
int b_read(b_io_fd fd, char *buffer, int count);
int b_write(b_io_fd fd, char *buffer, int count);
int b_seek(b_io_fd fd, off_t offset, int whence);
int b_close(b_io_fd fd);
```

### Directory Operations

```c
int fs_mkdir(const char *pathname, mode_t mode);
int fs_rmdir(const char *pathname);
fdDir *fs_opendir(const char *pathname);
struct fs_diriteminfo *fs_readdir(fdDir *dirp);
int fs_closedir(fdDir *dirp);

char *fs_getcwd(char *pathbuffer, size_t size);
int fs_setcwd(char *pathname);
int fs_isFile(char *filename);
int fs_isDir(char *pathname);
int fs_delete(char *filename);
```

#### Supporting Structures:

```c
struct fs_diriteminfo {
  unsigned short d_reclen;
  unsigned char fileType;
  char d_name[256];
};
```

### File Stat Operations

```c
int fs_stat(const char *filename, struct fs_stat *buf);

struct fs_stat {
  off_t st_size;
  blksize_t st_blksize;
  blkcnt_t st_blocks;
  time_t st_accesstime;
  time_t st_modtime;
  time_t st_createtime;

  /* Add custom filesystem attributes here */
};
```

These interfaces must be included and defined in your `mfs.h` header file.

## Development Environment

* **OS**: Ubuntu Linux VM
* **Language**: C
* **Compiler/Tools**: `gcc`, `make`
* **Max Line Length**: 80 characters (max 100)
* **Code Standards**:

  * Meaningful variable names (consistent style).
  * Well-documented, readable, and maintainable code.
  * Standard header in each file:

## Compilation and Execution

```bash
make          # Compile your filesystem
make run      # Run your filesystem shell
```

## Shell Commands (`fsshell.c`)

The provided shell (`fsshell.c`) supports the following commands (customizable):

| Command   | Description                        |
| --------- | ---------------------------------- |
| `ls`      | List files/directories             |
| `cp`      | Copy files within filesystem       |
| `mv`      | Move files                         |
| `md`      | Make directory                     |
| `rm`      | Remove file/directory              |
| `touch`   | Create empty file                  |
| `cat`     | Display file content               |
| `cp2l`    | Copy file from filesystem to Linux |
| `cp2fs`   | Copy file from Linux to filesystem |
| `cd`      | Change directory                   |
| `pwd`     | Show current directory             |
| `history` | Command history                    |
| `help`    | Help menu                          |

## Submission Requirements (via GitHub)

Your GitHub repository must include:

* Source files (`.c`, `.h`)
* Customized driver program (`fsshell.c`)
* `Makefile`
* Volume file (max 10MB)

Include a PDF Write-up with:

* GitHub repository link
* Design and implementation details per phase
* Explanation of filesystem internals
* Functionality overview with screenshots
* Issues and solutions encountered

Individual submissions:

* Individual report form (submitted separately via Canvas)

---

Ensure your GitHub repository reflects the full scope of your filesystem design, clearly documenting implementation choices and functionality.
