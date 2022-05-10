#ifndef _JUMBO_FILE_SYSTEM_H_
#define _JUMBO_FILE_SYSTEM_H_

#include "basic_file_system.h"


// maximum number of characters in a file or directory name (not counting '\0')
#define MAX_NAME_LENGTH 7

// maximum number of (combined total) files and subdirectories that can be in a directory
#define MAX_DIR_ENTRIES ((BLOCK_SIZE - sizeof(uint16_t) - sizeof(uint32_t)) / (sizeof(block_num_t) + MAX_NAME_LENGTH + 1))

// maximum number of data blocks that can be used to store a file
#define MAX_DATA_BLOCKS ((BLOCK_SIZE - sizeof(uint32_t) - sizeof(uint32_t)) / sizeof(block_num_t))

// maximum size (in bytes) that a file can be
#define MAX_FILE_SIZE (MAX_DATA_BLOCKS * BLOCK_SIZE)


// Struct returned by jfs_stat()
struct stats {
  uint32_t is_dir;                // 0 if it is a directory, 1 if it is a regular file
  char name[MAX_NAME_LENGTH + 1]; // +1 for the '\0' character
  block_num_t block_num;          // of the dir block, or the inode (for regular files)
  uint16_t num_data_blocks;       // not counting the inode (ignored if is_dir is 0)
  uint32_t file_size;             // in bytes (ignored if is_dir is 0)
};


// This is the data stored in an inode or directory block (dirnode)
struct block {
  uint32_t is_dir; // 0 if it is a directory, 1 if it is a regular file

  union {
    struct {
      uint32_t file_size; // in bytes
      block_num_t data_blocks[MAX_DATA_BLOCKS];
    } inode;

    struct {
      uint16_t num_entries; // must be <= MAX_DIR_ENTRIES
      struct {
        block_num_t block_num; // block where the file's inode or directory's dir block is stored
        char name[MAX_NAME_LENGTH + 1]; // +1 for the '\0' character
      } entries[MAX_DIR_ENTRIES];
    } dirnode;
  } contents;
};


// Function comments for all of these are in jumbo_file_system.c
int jfs_mount (const char* filename);

int jfs_mkdir (const char* directory_name);
int jfs_chdir (const char* directory_name);
int jfs_ls (char* directories[MAX_DIR_ENTRIES+1], char* files[MAX_DIR_ENTRIES+1]);
int jfs_rmdir (const char* directory_name);

int jfs_creat  (const char* file_name);
int jfs_remove (const char* file_name);
int jfs_stat   (const char* name, struct stats* buf);
int jfs_write  (const char* file_name, const void* buf, unsigned short count);
int jfs_read   (const char* file_name, void* buf, unsigned short* ptr_count);

int jfs_unmount();


// These are the error codes that jfs_* functions may return
#define E_SUCCESS 0
#define E_UNKNOWN -1
#define E_NOT_EXISTS -2      // the file or directory does not exist
#define E_EXISTS -3          // the file or directory already exists
#define E_NOT_DIR -4         // the name exists but is not a directory
#define E_IS_DIR -5          // the name exists but is a directory (not a regular file)
#define E_NOT_EMPTY -6       // the directory is not empty
#define E_MAX_NAME_LENGTH -7 // the name exceeds the maximum name length
#define E_MAX_DIR_ENTRIES -8 // the operation would cause the maximum number of entries in a directory to be exceeded
#define E_MAX_FILE_SIZE -9   // the operation would cause the maximum file size to be exceeded
#define E_DISK_FULL -10      // the disk is full (or the operation would require more capacity than remains on the disk)

#endif // _JUMBO_FILE_SYSTEM_H_
