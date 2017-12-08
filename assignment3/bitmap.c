// Free bitmap for OS file systems assignment

#include "bitmap.h"
#include <strings.h>    // For `ffs`

/* CONSTANTS */
// How far to loop in array
#define SIZE (NUM_BLOCKS/8) // 128
#define BITMAP_SIZE 4
#define BLOCK_SIZE 1024

/* GLOBALS */
// The actual data. Initialize all bits to high
// uint8_t free_bitmap[SIZE] = {[0 ... SIZE-1] = UINT8_MAX};
static char free_bitmap[BITMAP_SIZE * BLOCK_SIZE];

/* MACROS */
#define FREE_BIT(_data, _which_bit) \
        _data = _data | (1 << _which_bit)

#define USE_BIT(_data, _which_bit) \
        _data = _data & ~(1 << _which_bit)

void force_set_index(uint32_t index) {
    // TODO
    // Used to force indicies to used
    // This is the opposite of rm_index.

    // Get index in array of which bit to free
    uint32_t i = index / 8;

    // Get which bit to free
    uint8_t bit = index % 8;

    // Free bit
    USE_BIT(free_bitmap[i], bit);
}

uint32_t get_index() {
    uint32_t i = 0;

    // Find the first section with a free bit
    // Let's ignore overflow for now...
    while (free_bitmap[i] == 0)
        i++;

    // Now, find the first free bit
    /*
     * The ffs() function returns the position of the first (least
     * significant) bit set in the word i.  The least significant bit is
     * position 1 and the most significant position is, for example, 32 or
     * 64.
    */
    // Since ffs has the lsb as 1, not 0, we need to subtract
    uint8_t bit = ffs(free_bitmap[i]) - 1;

    // Set the bit to used
    USE_BIT(free_bitmap[i], bit);

    // Return which block we used
    return (i * 8) + bit;
}

void rm_index(uint32_t index) {

    // Get index in array of which bit to free
    uint32_t i = index / 8;

    // Get which bit to free
    uint8_t bit = index % 8;

    // Free bit
    FREE_BIT(free_bitmap[i], bit);
}

