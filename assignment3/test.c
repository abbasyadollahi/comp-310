#include "bitmap.h"

#include <assert.h>
#include <stdio.h>

int main() {

    /*
     * Get 64 blocks.
     * Since everything is empty, we should
     * get blocks 0..63
     */
    for (int i = 0; i < 64; i++) {
        assert(get_index() == i);
    }

    /*
     * Now, free some blocks
     */
    int to_free[3] = {0, 22, 37};
    for (int i = 0; i < 3; i++) {
        rm_index(to_free[i]);
    }

    /*
     * Get more blocks
     * Since we just freed 3, we should get
     * thos blocks back
     */
    for (int i = 0; i < 3; i++) {
        assert(get_index() == to_free[i]);
    }

    /*
     * Now, get some more blocks. Since all
     * 0..63 are allocated, we should now
     * get blocks 64..91
     */
    for (int i = 64; i < 92; i++) {
        assert(get_index() == i);
    }

    printf("All tests passed!\n");
}
