#include "basic_file_system.h"


int bfs_mount(const char* filename) {
  // mount the raw disk
  if (raw_mount(filename) < 0) {
    return -1;
  }

  // read the superblock
  char superblock[BLOCK_SIZE];
  if (read_block(0, superblock) < 0) {
    return -1;
  }

  // make sure the superblock and root directory are marked "allocated"
  if (!(superblock[0] & 3)) {
    superblock[0] |= 3;
    if (write_block(0, superblock) < 0) {
      return -1;
    }
  }
  return 0;
}


block_num_t allocate_block() {
  // read the superblock
  char superblock[BLOCK_SIZE];
  if (read_block(0, superblock) < 0) {
    return 0;
  }

  // find the first byte that is all allocated
  int byte;
  for (byte = 0;
       byte < BLOCK_SIZE && superblock[byte] == (char)-1;
       byte++) {}
  // if all bytes are all allocated, then there are no free blocks
  if (byte == BLOCK_SIZE) {
    return 0; // no free blocks
  }

  // find the bit index of the first 0 bit
  unsigned char field = superblock[byte];
  int bit;
  for (bit = 0; field & 1 && bit < 8; field >>= 1, bit++) {}
  if (bit == 8) {
    // this should be impossible because we checked that the byte is not all 1's
    return 0;
  }

  // set the found bit of the byte to 1
  superblock[byte] |= 1 << bit;

  // write the updated superblock back to disk
  if (write_block(0, superblock) < 0) {
    return 0;
  }
  return byte * 8 + bit;
}


int release_block(block_num_t block) {
  // read the superblock
  char superblock[BLOCK_SIZE];
  if (read_block(0, superblock) < 0) {
    return -1;
  }

  // change bit corresponding to block num to 0
  char mask = 1 << (block % 8);
  superblock[block / 8] &= ~mask;

  // write the updated superblock back to disk
  if (write_block(0, superblock) < 0) {
    return -1;
  }
  return 0;
}


int bfs_unmount() {
  return raw_unmount();
}
