#include "jumbo_file_system.h"
#include <string.h>
#include <stdlib.h>
#include <sys/stat.h>

// C does not have a bool type, so I created one that you can use
typedef char bool_t;
#define TRUE 1
#define FALSE 0

static block_num_t current_dir;

// optional helper function you can implement to tell you if a block is a dir node or an inode
static bool_t is_dir(block_num_t block_num) {
    char *buffer = malloc(BLOCK_SIZE);
    struct block *diskBlock = malloc(sizeof(struct block));
    bzero(buffer, BLOCK_SIZE);
    bzero(diskBlock, sizeof(struct block));
    read_block(block_num, buffer);
    memcpy(diskBlock, buffer, sizeof(struct block));
    if(diskBlock->is_dir==0){
      free(buffer);
      free(diskBlock);
      return TRUE;
    }
    else{
      free(buffer);
      free(diskBlock);
      return FALSE;
    }
}

static bool_t is_empty_dir(block_num_t block_num){
    char *buffer = malloc(BLOCK_SIZE);
    struct block *diskBlock = malloc(sizeof(struct block));
    bzero(buffer, BLOCK_SIZE);
    bzero(diskBlock, sizeof(struct block));
    read_block(block_num, buffer);
    memcpy(diskBlock, buffer, sizeof(struct block));
    if(diskBlock->contents.dirnode.num_entries==0){
      free(buffer);
      free(diskBlock);
      return TRUE;
    }
    else{
      free(buffer);
      free(diskBlock);
      return FALSE;
    }
}

int count_num_data_block(uint32_t file_size){
  if(file_size%BLOCK_SIZE==0){
    return file_size/BLOCK_SIZE;
  }
  else{
    return file_size/BLOCK_SIZE+1;
  }
}

int count_occupied_disk_size(int size, block_num_t block_num){
    char *buffer = malloc(BLOCK_SIZE);
    struct block *diskBlock = malloc(sizeof(struct block));
    bzero(buffer, BLOCK_SIZE);
    bzero(diskBlock, sizeof(struct block));
    read_block(block_num, buffer);
    memcpy(diskBlock, buffer, sizeof(struct block));
    if(diskBlock->is_dir==1) // it is a file
    {
      size+= count_num_data_block(diskBlock->contents.inode.file_size)*BLOCK_SIZE; //size of data blocks
    }
    else{ // it is a directory
      size+= diskBlock->contents.dirnode.num_entries*BLOCK_SIZE; //size of inode & directory blocks
      for(int i=0; i<diskBlock->contents.dirnode.num_entries; i++){
          size = count_occupied_disk_size(size, diskBlock->contents.dirnode.entries[i].block_num);
      }
    }
    free(diskBlock);
    free(buffer);
    return size;
}

block_num_t find_block_num_by_name(const char* directory_name){
    char *buffer = malloc(BLOCK_SIZE);
    struct block *dirBlock = malloc(sizeof(struct block));
    bzero(buffer, BLOCK_SIZE);
    bzero(dirBlock, sizeof(struct block));
    read_block(current_dir, buffer);
    memcpy(dirBlock, buffer, sizeof(struct block));
    uint16_t num_entries = dirBlock->contents.dirnode.num_entries;
    for(int i=0; i<num_entries; i++){
        if(!strcmp(dirBlock->contents.dirnode.entries[i].name, directory_name)){
            block_num_t block_num = dirBlock->contents.dirnode.entries[i].block_num;
            free(buffer);
            free(dirBlock);
            return block_num;
        }
    }
    free(buffer);
    free(dirBlock);
    return 0; // fail to find such block, return 0 (which is the block number of superblock, so there will be no confusion)
}

int rm_subdir_or_file_from_current_dir(const char* name){
    char *buffer = malloc(BLOCK_SIZE);
    struct block *dirBlock = malloc(sizeof(struct block));
    bzero(buffer, BLOCK_SIZE);
    bzero(dirBlock, sizeof(struct block));
    read_block(current_dir, buffer);
    memcpy(dirBlock, buffer, sizeof(struct block));
    uint16_t num_entries = dirBlock->contents.dirnode.num_entries;
    for(int i=0; i<num_entries; i++){
        if(!strcmp(dirBlock->contents.dirnode.entries[i].name, name)){
            // update block info 
            dirBlock->contents.dirnode.entries[i].block_num = dirBlock->contents.dirnode.entries[num_entries-1].block_num;
            bzero(dirBlock->contents.dirnode.entries[i].name, MAX_NAME_LENGTH);
            memcpy(dirBlock->contents.dirnode.entries[i].name, dirBlock->contents.dirnode.entries[num_entries-1].name, MAX_NAME_LENGTH);
            dirBlock->contents.dirnode.entries[num_entries-1].block_num = 0;
            bzero(dirBlock->contents.dirnode.entries[num_entries-1].name, MAX_NAME_LENGTH);
            dirBlock->contents.dirnode.num_entries--;
            // write back to disk
            bzero(buffer, BLOCK_SIZE);
            memcpy(buffer, dirBlock, sizeof(struct block));
            write_block(current_dir, buffer);
            free(buffer);
            free(dirBlock);
            return 0; // succeed
        }
    }
    free(buffer);
    free(dirBlock);
    return 1; // fail to find this directory or file
}

int create_inode_subdir_block(const char* name, int is_dir){
    // check if current directory is capable to create new sub-directory/inode
    if(strlen(name)>MAX_NAME_LENGTH){
      return E_MAX_NAME_LENGTH;
    }
    if(count_occupied_disk_size(2*BLOCK_SIZE, 1)+BLOCK_SIZE > BLOCK_SIZE*NUM_BLOCKS){// will exceed disk capacity after this (initial size: 2*BLOCK_SIZE accounts for superblock and root directory block)
      return E_DISK_FULL;
    }
    char *buffer = malloc(BLOCK_SIZE);
    struct block *dirBlock = malloc(sizeof(struct block));
    bzero(buffer, BLOCK_SIZE);
    bzero(dirBlock, sizeof(struct block));
    read_block(current_dir, buffer);
    memcpy(dirBlock, buffer, sizeof(struct block));
    if(dirBlock->contents.dirnode.num_entries>=MAX_DIR_ENTRIES){
      free(dirBlock);
      free(buffer);
      return E_MAX_DIR_ENTRIES;
    }
    for(int i=0; i<dirBlock->contents.dirnode.num_entries; i++){
      if(!strcmp(dirBlock->contents.dirnode.entries[i].name,name)){
        free(dirBlock);
        free(buffer);
        return E_EXISTS;
      }
    }
    // allocate new block for sub-directory/inode
    block_num_t dirNum = allocate_block();
    // update current directory info
    dirBlock->contents.dirnode.entries[dirBlock->contents.dirnode.num_entries].block_num = dirNum;
    memcpy(dirBlock->contents.dirnode.entries[dirBlock->contents.dirnode.num_entries].name, name, MAX_NAME_LENGTH);
    dirBlock->contents.dirnode.num_entries++;
    bzero(buffer, BLOCK_SIZE);
    memcpy(buffer, dirBlock, sizeof(struct block));
    write_block(current_dir, buffer);
    // store sub-directory/inode info
    bzero(buffer, BLOCK_SIZE);
    bzero(dirBlock, sizeof(struct block));
    dirBlock->is_dir=is_dir;
    memcpy(buffer, dirBlock, sizeof(struct block));
    write_block(dirNum, buffer);
    // free pointers
    free(dirBlock);
    free(buffer);
    return 0;
}

/* jfs_mount
 *   prepares the DISK file on the _real_ file system to have file system
 *   blocks read and written to it.  The application _must_ call this function
 *   exactly once before calling any other jfs_* functions.  If your code
 *   requires any additional one-time initialization before any other jfs_*
 *   functions are called, you can add it here.
 * filename - the name of the DISK file on the _real_ file system
 * returns 0 on success or -1 on error; errors should only occur due to
 *   errors in the underlying disk syscalls.
 */
int jfs_mount(const char* filename) {
    int ret = bfs_mount(filename);
    current_dir = 1;
    return ret;
}

/* jfs_mkdir
 *   creates a new subdirectory in the current directory
 * directory_name - name of the new subdirectory
 * returns 0 on success or one of the following error codes on failure:
 *   E_EXISTS, E_MAX_NAME_LENGTH, E_MAX_DIR_ENTRIES, E_DISK_FULL
 */
int jfs_mkdir(const char* directory_name) {
    return create_inode_subdir_block(directory_name, 0);
}

/* jfs_chdir
 *   changes the current directory to the specified subdirectory, or changes
 *   the current directory to the root directory if the directory_name is NULL
 * directory_name - name of the subdirectory to make the current
 *   directory; if directory_name is NULL then the current directory
 *   should be made the root directory instead
 * returns 0 on success or one of the following error codes on failure:
 *   E_NOT_EXISTS, E_NOT_DIR
 */
int jfs_chdir(const char* directory_name) {
    if(directory_name==NULL){
      current_dir = 1; //change to root directory
      return 0;
    }
    block_num_t block_num = find_block_num_by_name(directory_name);
    if(block_num==0){
      return E_NOT_EXISTS;
    }
    else if(!is_dir(block_num)){ // if this is a file
      return E_NOT_DIR;
    }
    else{ // if this is a directory
      current_dir = block_num;
      return 0;
    }
}

/* jfs_ls
 *   finds the names of all the files and directories in the current directory
 *   and writes the directory names to the directories argument and the file
 *   names to the files argument
 * directories - array of strings; the function will set the strings in the
 *   array, followed by a NULL pointer after the last valid string; the strings
 *   should be malloced and the caller will free them
 * file - array of strings; the function will set the strings in the
 *   array, followed by a NULL pointer after the last valid string; the strings
 *   should be malloced and the caller will free them
 * returns 0 on success or one of the following error codes on failure:
 *   (this function should always succeed)
 */
int jfs_ls(char* directories[MAX_DIR_ENTRIES+1], char* files[MAX_DIR_ENTRIES+1]) {
    char *buffer = malloc(BLOCK_SIZE);
    struct block *dirBlock = malloc(sizeof(struct block));
    bzero(buffer, BLOCK_SIZE);
    bzero(dirBlock, sizeof(struct block));
    read_block(current_dir, buffer);
    memcpy(dirBlock, buffer, sizeof(struct block));
    uint16_t num_entries = dirBlock->contents.dirnode.num_entries;
    int dirCount=0;
    int filecount=0;
    for(int i=0; i<num_entries; i++){
        if(is_dir(dirBlock->contents.dirnode.entries[i].block_num)){ // if this is a directory
          directories[dirCount] = malloc(MAX_NAME_LENGTH);
          memcpy(directories[dirCount],dirBlock->contents.dirnode.entries[i].name,MAX_NAME_LENGTH); 
          dirCount++;
        }
        else{ // if this is a regular file
          files[filecount] = malloc(MAX_NAME_LENGTH);
          memcpy(files[filecount],dirBlock->contents.dirnode.entries[i].name,MAX_NAME_LENGTH); 
          filecount++;
        }
    }
    // set the rest of arrays NULL
    for(unsigned long i=dirCount; i<MAX_DIR_ENTRIES+1; i++){
      directories[i]=NULL;
    }
    for(unsigned long i=filecount; i<MAX_DIR_ENTRIES+1; i++){
      files[i]=NULL;
    }
    // free pointers
    free(dirBlock);
    free(buffer);
    return 0;
}

/* jfs_rmdir
 *   removes the specified subdirectory of the current directory
 * directory_name - name of the subdirectory to remove
 * returns 0 on success or one of the following error codes on failure:
 *   E_NOT_EXISTS, E_NOT_DIR, E_NOT_EMPTY
 */
int jfs_rmdir(const char* directory_name) {
    block_num_t block_num = find_block_num_by_name(directory_name);
    if(block_num==0){
      return E_NOT_EXISTS;
    }
    else if(!is_dir(block_num)){ // if this is a file
      return E_NOT_DIR;
    }
    else{ // if this is a directory
      if(!is_empty_dir(block_num)){
        return E_NOT_EMPTY;
      }
      else{
        rm_subdir_or_file_from_current_dir(directory_name);
        release_block(block_num);
        return 0;
      }
    }
}

/* jfs_creat
 *   creates a new, empty file with the specified name
 * file_name - name to give the new file
 * returns 0 on success or one of the following error codes on failure:
 *   E_EXISTS, E_MAX_NAME_LENGTH, E_MAX_DIR_ENTRIES, E_DISK_FULL
 */
int jfs_creat(const char* file_name) {
    return create_inode_subdir_block(file_name, 1);
}

/* jfs_remove
 *   deletes the specified file and all its data (note that this cannot delete
 *   directories; use rmdir instead to remove directories)
 * file_name - name of the file to remove
 * returns 0 on success or one of the following error codes on failure:
 *   E_NOT_EXISTS, E_IS_DIR
 */
int jfs_remove(const char* file_name) {
    block_num_t block_num = find_block_num_by_name(file_name);
    if(block_num==0){
      return E_NOT_EXISTS;
    }
    else if(is_dir(block_num)){ // if this is a directory
      return E_IS_DIR;
    }
    else{ // if this is a file
      rm_subdir_or_file_from_current_dir(file_name);
      // read inode info and release all data blocks
      char *buffer = malloc(BLOCK_SIZE);
      struct block *dirBlock = malloc(sizeof(struct block));
      bzero(buffer, BLOCK_SIZE);
      bzero(dirBlock, sizeof(struct block));
      read_block(block_num, buffer);
      memcpy(dirBlock, buffer, sizeof(struct block));
      int num_data_blocks = count_num_data_block(dirBlock->contents.inode.file_size);
      for(int i=0; i<num_data_blocks; i++){
        release_block(dirBlock->contents.inode.data_blocks[i]);
      }
      // release inode block
      release_block(block_num);
      // free pointers
      free(dirBlock);
      free(buffer);
      return 0;
    }
}

/* jfs_stat
 *   returns the file or directory stats (see struct stat for details)
 * name - name of the file or directory to inspect
 * buf  - pointer to a struct stat (already allocated by the caller) where the
 *   stats will be written
 * returns 0 on success or one of the following error codes on failure:
 *   E_NOT_EXISTS
 */
int jfs_stat(const char* name, struct stats* buf) {
    int block_num = find_block_num_by_name(name);
    if(block_num==0){
      return E_NOT_EXISTS;
    }
    memcpy(buf->name, name, MAX_NAME_LENGTH);
    buf->block_num = block_num;
    if(!is_dir(block_num)){ // if this is a file 
      buf->is_dir = 1;
      // read inode info
      char *buffer = malloc(BLOCK_SIZE);
      struct block *dirBlock = malloc(sizeof(struct block));
      bzero(buffer, BLOCK_SIZE);
      bzero(dirBlock, sizeof(struct block));
      read_block(block_num, buffer);
      memcpy(dirBlock, buffer, sizeof(struct block));
      uint32_t file_size = dirBlock->contents.inode.file_size;
      uint16_t num_data_blocks = count_num_data_block(file_size);
      buf->file_size = file_size;
      buf->num_data_blocks = num_data_blocks;
      // free pointers
      free(dirBlock);
      free(buffer); 
    }
    else{ // if this is a directory
      buf->is_dir = 0;
    }
    return 0;
}

/* jfs_write
 *   appends the data in the buffer to the end of the specified file
 * file_name - name of the file to append data to
 * buf - buffer containing the data to be written (note that the data could be
 *   binary, not text, and even if it is text should not be assumed to be null
 *   terminated)
 * count - number of bytes in buf (write exactly this many)
 * returns 0 on success or one of the following error codes on failure:
 *   E_NOT_EXISTS, E_IS_DIR, E_MAX_FILE_SIZE, E_DISK_FULL
 */
int jfs_write(const char* file_name, const void* buf, unsigned short count) {
    int block_num = find_block_num_by_name(file_name);
    if(block_num==0){
      return E_NOT_EXISTS;
    }
    if(is_dir(block_num)){ // if this is a directory
      return E_IS_DIR;
    }
    // read inode info
    char *buffer = malloc(BLOCK_SIZE);
    struct block *dirBlock = malloc(sizeof(struct block));
    bzero(buffer, BLOCK_SIZE);
    bzero(dirBlock, sizeof(struct block));
    read_block(block_num, buffer);
    memcpy(dirBlock, buffer, sizeof(struct block));
    uint32_t o_file_size = dirBlock->contents.inode.file_size;
    if(o_file_size+count>MAX_FILE_SIZE){
        // free pointers
        free(dirBlock);
        free(buffer); 
        return E_MAX_FILE_SIZE;      
    }
    uint16_t o_num_data_blocks = count_num_data_block(o_file_size);
    uint16_t add_num_data_blocks;
    if(count<=o_num_data_blocks*BLOCK_SIZE-o_file_size){
      add_num_data_blocks=0;
    }
    else{
      add_num_data_blocks = count_num_data_block(count-(o_num_data_blocks*BLOCK_SIZE-o_file_size));
    }
    if(count_occupied_disk_size(2*BLOCK_SIZE, 1)+add_num_data_blocks*BLOCK_SIZE > BLOCK_SIZE*NUM_BLOCKS){
      // free pointers
      free(dirBlock);
      free(buffer); 
      return E_DISK_FULL;
    }
    // allocate new data blocks and update inode info
    dirBlock->contents.inode.file_size = o_file_size+count;
    for(int i=0; i<add_num_data_blocks; i++){
      block_num_t dirNum = allocate_block();
      dirBlock->contents.inode.data_blocks[i+o_num_data_blocks]=dirNum;
    }
    bzero(buffer, BLOCK_SIZE);
    memcpy(buffer, dirBlock, sizeof(struct block));
    write_block(block_num,buffer);
    // append data to data blocks
    if(o_num_data_blocks*BLOCK_SIZE-o_file_size==0){ //if there is no partial block in original data blocks
        for(int i=0; i<add_num_data_blocks; i++){
          write_block(dirBlock->contents.inode.data_blocks[i+o_num_data_blocks],buf+i*BLOCK_SIZE);
        }
    }
    else{ //if there is a partial block in original data blocks
        void *new_buffer = malloc(BLOCK_SIZE);
        block_num_t partial_block_num = dirBlock->contents.inode.data_blocks[o_num_data_blocks-1];
        read_block(partial_block_num,new_buffer);
        if(count<=o_num_data_blocks*BLOCK_SIZE-o_file_size){ //left space in the partial block is enough
          memcpy(new_buffer+o_file_size-(o_num_data_blocks-1)*BLOCK_SIZE, buf, count); //append to the partial block
          write_block(partial_block_num,new_buffer);
        }
        else{ //need to first append to the partial block, then append to extra data blocks
            // append to the partial block
            uint32_t offset = o_num_data_blocks*BLOCK_SIZE-o_file_size;
            memcpy(new_buffer+o_file_size-(o_num_data_blocks-1)*BLOCK_SIZE, buf, offset);
            write_block(partial_block_num,new_buffer);
            // append to extra data blocks
            for(int i=0; i<add_num_data_blocks; i++){
              write_block(dirBlock->contents.inode.data_blocks[i+o_num_data_blocks],buf+offset+i*BLOCK_SIZE);
            }
        }
        free(new_buffer);
    }
    // free pointers
    free(dirBlock);
    free(buffer); 
    return 0;
}


/* jfs_read
 *   reads the specified file and copies its contents into the buffer, up to a
 *   maximum of *ptr_count bytes copied (but obviously no more than the file
 *   size, either)
 * file_name - name of the file to read
 * buf - buffer where the file data should be written
 * ptr_count - pointer to a count variable (allocated by the caller) that
 *   contains the size of buf when it's passed in, and will be modified to
 *   contain the number of bytes actually written to buf (e.g., if the file is
 *   smaller than the buffer) if this function is successful
 * returns 0 on success or one of the following error codes on failure:
 *   E_NOT_EXISTS, E_IS_DIR
 */
int jfs_read(const char* file_name, void* buf, unsigned short* ptr_count) {
    int block_num = find_block_num_by_name(file_name);
    if(block_num==0){
      return E_NOT_EXISTS;
    }
    if(is_dir(block_num)){ // if this is a directory
      return E_IS_DIR;
    }
    // read inode info
    char *buffer = malloc(BLOCK_SIZE);
    struct block *dirBlock = malloc(sizeof(struct block));
    bzero(buffer, BLOCK_SIZE);
    bzero(dirBlock, sizeof(struct block));
    read_block(block_num, buffer);
    memcpy(dirBlock, buffer, sizeof(struct block));
    uint32_t file_size = dirBlock->contents.inode.file_size;
    uint16_t num_data_blocks = count_num_data_block(file_size);
    *ptr_count = file_size;
    for(int i=0;i<num_data_blocks;i++){
      bzero(buffer, BLOCK_SIZE);
      read_block(dirBlock->contents.inode.data_blocks[i],buffer);
      if(i==num_data_blocks-1){memcpy(buf+i*BLOCK_SIZE,buffer,file_size-(num_data_blocks-1)*BLOCK_SIZE);}
      else{memcpy(buf+i*BLOCK_SIZE,buffer,BLOCK_SIZE);}
    }
    // free pointers
    free(dirBlock);
    free(buffer); 
    return 0;
}


/* jfs_unmount
 *   makes the file system no longer accessible (unless it is mounted again).
 *   This should be called exactly once after all other jfs_* operations are
 *   complete; it is invalid to call any other jfs_* function (except
 *   jfs_mount) after this function complete.  Basically, this closes the DISK
 *   file on the _real_ file system.  If your code requires any clean up after
 *   all other jfs_* functions are done, you may add it here.
 * returns 0 on success or -1 on error; errors should only occur due to
 *   errors in the underlying disk syscalls.
 */
int jfs_unmount() {
  int ret = bfs_unmount();
  return ret;
}