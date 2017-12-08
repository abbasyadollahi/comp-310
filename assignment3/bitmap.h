#ifndef _INCLUDE_BITMAP_H_
#define _INCLUDE_BITMAP_H_

#include <stdint.h>

#define NUM_BLOCKS 1024

/*
 * @short   Force an index to be set.
 * @long    Use this to setup your superblock, inode table and free bit map
 *          This has been left unimplemented. You should fill it out.
 * @param   index: Index to set
 */
void force_set_index(uint32_t index);

/*
 * @short   Find the first free data block
 * @return  Index of data block to use
 */
uint32_t get_index();

/*
 * @short   Frees an index
 * @param   index: Index to free
 */
void rm_index(uint32_t index);

#endif //_INCLUDE_BITMAP_H_


