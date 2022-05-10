#ifndef _BASIC_FILE_SYSTEM_H_
#define _BASIC_FILE_SYSTEM_H_

#include "raw_disk.h"

int bfs_mount(const char* filename);

/* allocate_block
 *   allocates a new block - finds a block that not yet allocated, marks it as
 *   allocated, and returns its block number - blocks marked as allocated will
 *   not be returned by allocate_block() again, unless they are first released
 *   by calling release_block()
 * returns the block number of the allocated block on succes, or 0 on failure
 * (failure may be assumed to mean that all blocks on the disk are already
 *  allocated)
 */
block_num_t allocate_block();

/* release_block
 *   releases the specified disk block, allowing it to be allocated again by
 *   allocate_block() sometime in the future
 * block - number of the block to release
 * returns 0 on success and -1 on failure
 * (Failure of release_block() should only happen if there is an error
 *  accessing the underlying _real_ file system.  Releasing a block that is
 *  not allocated is _not_ an error; it's just a no-op.)
 */
int release_block(block_num_t block);

int bfs_unmount();

#endif // _BASIC_FILE_SYSTEM_H_
