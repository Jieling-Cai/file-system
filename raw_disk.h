#ifndef _RAW_DISK_H_
#define _RAW_DISK_H_

#include <stdint.h>

#define BLOCK_SIZE 64
#define NUM_BLOCKS (8 * BLOCK_SIZE)

// block_num_t is the data type for a block number
// and is a 16-bit unsigned integer
typedef uint16_t block_num_t;


int raw_mount(const char* filename);

/* read_block
 *   reads a block from the disk
 * block_num - number of the block to read
 * buf - data read from disk will be copied into this buffer
 * (precondition: buf is BLOCK_SIZE bytes long)
 * returns 0 on success or -1 on failure
 */
int read_block(block_num_t block_num, void* buf);

/* write_block
 *   writes a block to the disk
 * block_num - number of the block to write
 * buf - buffer containing the data to write to disk
 * (precondition: buf is BLOCK_SIZE bytes long)
 * return 0 on success or -1 on failure
 */
int write_block(block_num_t block_num, void* buf);

int raw_unmount();

#endif // _RAW_DISK_H_

