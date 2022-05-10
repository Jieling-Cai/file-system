#include "raw_disk.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>

static const char* disk_filename = NULL;
static int disk_fd = -1;


int raw_mount(const char* filename) {
  // open file; creat if it doesn't exist already
  disk_fd = open(filename, O_CREAT|O_RDWR, S_IRUSR|S_IWUSR);
  if (disk_fd < 0) {
    return -1;
  }

  // check the file size
  off_t file_size = lseek(disk_fd, 0, SEEK_END);
  if (file_size < 0) {
    close(disk_fd);
    disk_fd = -1;
    return -1;

  } else if (file_size < NUM_BLOCKS * BLOCK_SIZE) {
    // if the file size is less than it should be, we need to extend it
    long to_write = NUM_BLOCKS * BLOCK_SIZE - file_size;
    char* buffer = (char*) malloc(to_write * sizeof(char));
    // make sure the new allocation writes 0's to the disk
    for (int i = 0; i < to_write; i++) {
      buffer[i] = 0;
    }

    // commit the file extension to disk
    if (write(disk_fd, buffer, to_write) < to_write) {
      close(disk_fd);
      disk_fd = -1;
      free(buffer);
      return -1;
    }
    free(buffer);
  }

  disk_filename = filename;
  return 0;
}


int read_block(block_num_t block_num, void* buf) {
  // go to the block
  if (lseek(disk_fd, block_num * BLOCK_SIZE, SEEK_SET) < 0) {
    return -1;
  }
  // read the block
  int ret = read(disk_fd, buf, BLOCK_SIZE);
  if (ret != BLOCK_SIZE) {
    return -1;
  }
  return 0;
}


int write_block(block_num_t block_num, void* buf) {
  // go to the block
  if (lseek(disk_fd, block_num * BLOCK_SIZE, SEEK_SET) < 0) {
    return -1;
  }
  // write the block
  int ret = write(disk_fd, buf, BLOCK_SIZE);
  if (ret != BLOCK_SIZE) {
    return -1;
  }
  return 0;
}


int raw_unmount() {
  disk_filename = NULL;
  return close(disk_fd);
}
