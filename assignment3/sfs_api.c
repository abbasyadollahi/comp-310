#include <fuse.h>
#include <math.h>
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <strings.h>
#include <sys/types.h>

#include "bitmap.h"
#include "sfs_api.h"
#include "disk_emu.h"

#define FILES_PER_BLOCK 42
#define INODES_PER_BLOCK 16

#define INODE_SIZE 7
#define INODE_START_ADDR 1
#define DIR_SIZE 3
#define DIR_START_ADDR INODE_START_ADDR + INODE_SIZE
#define BITMAP_SIZE 4
#define BITMAP_START_ADDR MAX_NUM_BLOCKS - BITMAP_SIZE
#define DATA_START_ADDR DIR_START_ADDR + DIR_SIZE
#define DATA_BLOCKS MAX_NUM_BLOCKS - DATA_START_ADDR - BITMAP_SIZE

#define ROOT_IDX 0
#define MAGIC_NUMBER 0xACBD0005
#define SFS_DISK "sfs_disk_abbas.disk"
#define MAX_FILES 100
#define MAX_OPEN_FILES 64
#define BLOCK_SIZE 1024                         // Size of one data block.
#define MAX_NUM_BLOCKS 1024                     // Max number of data blocks on the disk.
#define BITMAP_ROW_SIZE (MAX_NUM_BLOCKS/8)      // Mimcs the number of rows in the bitmap.
#define MAX_FILE_SIZE BLOCK_SIZE * (DATA_POINTERS + (BLOCK_SIZE / 4))

// Abbas Yadollahi
// 260680343

// After testing several times, I couldn't figure out why it would abort during the sfs_open() call.
// For test1, it seem to work once every 5 tries or so, so I'm assuming as specific test might be
// trying to write too many bytes for file.
// For test2, it works more often without aborting, although it still seems to fail at some specific
// test cases. Unfortunately ran out of time, so I didn't get to debug it.


// ==========
// | MACROS |
// ==========

#define FREE_BIT(_data, _which_bit) \
        _data = _data | (1 << _which_bit)

#define USE_BIT(_data, _which_bit) \
        _data = _data & ~(1 << _which_bit)

#define MIN(x, y) \
        x < y ? x : y

#define MAX(x, y) \
        x > y ? x : y

// =========
// | SETUP |
// =========

// Initialize all bits to high
// uint8_t free_bitmap[BITMAP_ROW_SIZE] = {[0 ... BITMAP_ROW_SIZE - 1] = UINT8_MAX};

// Initialize in memory cache
static char free_bitmap[BITMAP_SIZE*BLOCK_SIZE];
static dir_entry_t root_dir[MAX_FILES];     // Directory cache
inode_t inode_table[MAX_FILES];             // Inode cache
fd_t fd_table[MAX_OPEN_FILES];              // File descriptor cache

// Initialize globals
int file_idx = 0;
const char root_name[5] = "root/";


// ==================
// | MAIN FUNCTIONS |
// ==================

// Make a Simple File System
// Inputs:  int fresh   - 0 to reuse existing SFS, 1 to create new SFS
void mksfs(int fresh) {
    int i;

    // Initialize file descriptor table
    fd_t *tmp_fd = calloc(1, sizeof(fd_t));
    tmp_fd->inode_idx = 1;
    tmp_fd->inode = NULL;
    tmp_fd->free = 1;
    tmp_fd->rw_ptr = 0;

    for (i = 0; i < MAX_OPEN_FILES; i++)
        memcpy((void *)&fd_table[i], (void *)tmp_fd, sizeof(fd_t));

    if (fresh) {
        // Initialize new simple file system
        printf("Initiliazing SFS.\n");
        init_fresh_disk(SFS_DISK, BLOCK_SIZE, MAX_NUM_BLOCKS);

        // Clear all cache
        bzero(&root_dir, sizeof(dir_entry_t) * MAX_FILES);
        bzero(&inode_table, sizeof(inode_t) * MAX_FILES);

        // Initialize super block
        superblock_t *superblock = calloc(1, sizeof(superblock_t));
        superblock->magic = MAGIC_NUMBER;
        superblock->block_size = BLOCK_SIZE;
        superblock->inode_table_len = INODE_SIZE;
        superblock->root_dir_inode = ROOT_IDX;

        // Write super block
        printf("Writing Super block.\n");
        char buffer[BLOCK_SIZE];
        memcpy((void *)buffer, (void *)superblock, sizeof(superblock_t));
        write_blocks(ROOT_IDX, 1, (void *)buffer);

        // Initialize directory inode
        inode_t *tmp_inode = calloc(1, sizeof(inode_t));
        tmp_inode->mode = 755;      //RWX
        tmp_inode->linked = 1;
        tmp_inode->size = 0;        // For size of root dir, store number of bytes in currently in dir (not including root dir file) = sizeof(dir_entry_t)*(num files in dir)
        for (i = 0; i < DIR_SIZE; i++)                      // DIR_SIZE < DATA_POINTERS, so this is ok
            tmp_inode->data_ptrs[i] = DIR_START_ADDR + i; // Dir is pre-allocated
        memcpy((void *)&inode_table[ROOT_IDX], (void *)tmp_inode, sizeof(inode_t));

        // Write directory inode
        printf("Writing Directory Inode.\n");
        write_inode(ROOT_IDX);

        // Initialize root directory
        dir_entry_t *tmp_dir = calloc(1, sizeof(dir_entry_t));
        tmp_dir = &root_dir[ROOT_IDX];
        strcpy(tmp_dir->name, root_name);
        tmp_dir->inode_idx = ROOT_IDX;
        memcpy((void *)&root_dir[ROOT_IDX], (void *)&tmp_dir, sizeof(dir_entry_t));

        // Write root directory
        printf("Writing Root Directory.\n");
        write_dir_entry(ROOT_IDX);

        // Initialize data bitmap
        memset((void *)&free_bitmap, 0, DIR_SIZE);
        for (i = DIR_SIZE; i < (BITMAP_SIZE * BLOCK_SIZE); i++)
            free_bitmap[ROOT_IDX + i] = 1;

        // Write data bitmap
        printf("Writing Bitmap.\n");
        write_bitmap(ROOT_IDX);

        printf("Disk initialized.\n");
    } else {
        // Open existing simple file system
        printf("Opening SFS.\n");
        init_disk(SFS_DISK, BLOCK_SIZE, MAX_NUM_BLOCKS);

        // Read inodes into cache
        printf("Loading Inodes into cache.\n");
        inode_t *inodes_tmp;
        char buffer_inodes[INODE_SIZE * BLOCK_SIZE];
        read_blocks(INODE_START_ADDR, INODE_SIZE, (void *)buffer_inodes);
        inodes_tmp = (inode_t *)buffer_inodes;
        memcpy((void *)inode_table, (void *)inodes_tmp, MAX_FILES * sizeof(inode_t));

        // Read directory into cache
        printf("Loading Directory into cache.\n");
        dir_entry_t *dir_tmp;
        char buffer_dir[DIR_SIZE * BLOCK_SIZE];
        read_blocks(DIR_START_ADDR, DIR_SIZE, (void *)buffer_dir);
        dir_tmp = (dir_entry_t *)buffer_dir;
        memcpy((void *)root_dir, (void *)dir_tmp, MAX_FILES * sizeof(dir_entry_t));

        // Read data bitmap into cache
        printf("Loading Bitmap into cache.\n");
        char buffer_bitmap[BITMAP_SIZE * BLOCK_SIZE];
        read_blocks(BITMAP_START_ADDR, BITMAP_SIZE, (void *)buffer_bitmap);
        memcpy((void *)free_bitmap, (void *)buffer_bitmap, BLOCK_SIZE * BITMAP_SIZE);
    }

    return;
}

// Get the name of the next file in the directory
// Inputs:  char *fname - Copy name of file into fname
// Returns: int - 0 after returning all files, index of file in root directory otherwise
int sfs_getnextfilename(char *fname) {
    int num_files = get_num_files();

    if (file_idx < num_files) {
        strcpy(fname, root_dir[file_idx].name);
        return file_idx++;
    } else {
        file_idx = 0;
        return file_idx;
    }
}

// Get the size of a file in the directory
// Inputs:  char* path  - Path of the file
// Returns: int - Size of file, -1 if path doesn't exist
int sfs_getfilesize(const char* path) {
    int inode_idx = get_inode(path);

    return inode_idx != -1 ? inode_table[inode_idx].size : inode_idx;
}

// Open a file with the give name if it exists, otherwise create it
// Inputs:  char *name  - Name of the file
// Returns: int - Index of the file (fileID) in the fd table, -1 if unsuccessful
int sfs_fopen(char *name) {
    int i;

    // Check if there's already too many files open
    int num_files = get_num_files();

    if (num_files >= MAX_FILES)
        return -1;

    // Check for over sized file name
    char *extension = strdup(name);
    char *file_name = strsep(&extension, ".");

    if (file_name == NULL || strlen(file_name) > MAX_FILE_NAME)
        return -1;
    if (extension == NULL || strlen(extension) > MAX_EXT_NAME)
        return -1;

    // Find free fd in fd table
    int fd_idx = -1;
    for (i = 0; i < MAX_OPEN_FILES; i++) {
        if (fd_table[i].free == 1) {
            fd_idx = i;
            break;
        }
    }

    if (fd_idx == -1)
        return -1;

    // Check if file is already open
    int inode_idx = get_inode(name);
    for (i = 0; i < MAX_OPEN_FILES; i++) {
        if (fd_table[i].inode_idx == inode_idx)
            return -1;
    }

    // If not, create the file
    if (inode_idx == -1) {
        for (i = 0; i < MAX_FILES; i++) {
            if (inode_table[i].linked == 0) {
                inode_idx = i;
                break;
            }
        }

        // All inodes are already being used
        if (inode_idx == -1)
            return -1;

        // Look for free block in bitmap
        int bitmap_idx;
        for (bitmap_idx = 0; bitmap_idx < DATA_BLOCKS; bitmap_idx++) {
            if (free_bitmap[bitmap_idx] == 1) {
                memset((void *)&free_bitmap[bitmap_idx], 0, sizeof(char));
                break;
            }
        }

        // All blocks are being used
        if (bitmap_idx == DATA_BLOCKS)
            return -1;
        // Write to data bitmap
        else
            write_bitmap(bitmap_idx);

        // Open new file and add to directory
        void *pointer = (void *)&root_dir[num_files + 1];
        dir_entry_t *new_file = calloc(1, sizeof(dir_entry_t));
        strcpy(new_file->name, name);
        new_file->inode_idx = inode_idx;
        memcpy((void *)pointer, (void *)new_file, sizeof(dir_entry_t));
        free(new_file);
        write_dir_entry(num_files + 1);

        // Increase size of root inode
        inode_t *tmp_inode = calloc(1, sizeof(inode_t));
        tmp_inode = &inode_table[ROOT_IDX];
        tmp_inode->size += sizeof(dir_entry_t);
        memcpy((void *)&inode_table[ROOT_IDX], (void *)tmp_inode, sizeof(inode_t));
        write_inode(ROOT_IDX);

        // Link inode to new file
        tmp_inode = &inode_table[inode_idx];
        tmp_inode->data_ptrs[0] = bitmap_idx;
        tmp_inode->mode = 0x755;
        tmp_inode->linked = 1;
        tmp_inode->size = 0;
        memcpy((void *)&inode_table[inode_idx], (void *)tmp_inode, sizeof(inode_t));

        if (inode_idx > INODES_PER_BLOCK)
            write_inode(inode_idx);
    }

    // Add new file in fd table
    fd_t *tmp_fd = calloc(1, sizeof(fd_t));
    tmp_fd->rw_ptr = 0;
    tmp_fd->inode_idx = inode_idx;
    tmp_fd->free = 0;
    tmp_fd->rw_ptr = 0; //set to 0 automatically upon opening
    memcpy((void *)&fd_table[fd_idx], (void *)tmp_fd, sizeof(fd_t));

    return fd_idx;
}

// Close a file and remove its fd from the table
// Inputs:  int fileID  - Index of the file in the fd table
// Returns: int - 0 if file closes, -1 if file doesn't exist
int sfs_fclose(int fileID) {
    if (fileID < 0 || fileID > MAX_OPEN_FILES - 1)
        return -1;

    if (fd_table[fileID].free == 1)
        return -1;

    // Clear the file descriptor
    fd_t *rst_fd = calloc(1, sizeof(fd_t));
    rst_fd->inode_idx = -1;
    rst_fd->inode = NULL;
    rst_fd->free = 1;
    rst_fd->rw_ptr = 0;
    memcpy((void *)&fd_table[fileID], (void *)rst_fd, sizeof(fd_t));

    // Clear the inode
    inode_t *rst_inode = calloc(1, sizeof(inode_t));
    rst_inode = &inode_table[ROOT_IDX];
    rst_inode->size -= sizeof(dir_entry_t);
    memcpy((void *)&inode_table[ROOT_IDX], (void *)rst_inode, sizeof(inode_t));
    write_inode(ROOT_IDX);

    return 0;
}

// Reads the specified amount of data from the file, starting from the file pointer, and puts it into buf
// Inputs:  int fileID  - Index of the file in the fd table
//          char *buf   - Buffer to store the data being read
//          int length  - Number of bytes to be read
// Return:  int - 1 if successfully read file, 0 otherwise
int sfs_fread(int fileID, char *buf, int length) {
    if (fileID < 0 || fileID > MAX_OPEN_FILES - 1)
        return -1;

    // Make sure file is open
    if (fd_table[fileID].free == 1)
        return -1;

    int inode_idx = fd_table[fileID].inode_idx;
    int ptr_pos = fd_table[fileID].rw_ptr;

    // Reduce length if longer than file size or rw pointer position
    if ((ptr_pos + length) > inode_table[inode_idx].size)
        length = inode_table[inode_idx].size - ptr_pos;

    int block_idx = ptr_pos / BLOCK_SIZE;
    ptr_pos = ptr_pos % BLOCK_SIZE;
    int block_ptr = get_block_ptr_offset(inode_idx, block_idx);
    if (block_ptr == -1)
        return -1;

    char block[BLOCK_SIZE];
    read_blocks(DATA_START_ADDR + block_ptr, 1, (void *)block);

    // Data to read is all in one block
    if ((ptr_pos + length) <= BLOCK_SIZE) {
        memcpy((void *)buf, (void *)&block[ptr_pos], length);
        fd_t *tmp_fd = calloc(1, sizeof(fd_t));
        tmp_fd = &fd_table[fileID];
        tmp_fd->rw_ptr += length;
        memcpy((void *)&fd_table[fileID], (void *)tmp_fd, sizeof(fd_t));
        return length;
    }

    int last_full_block_idx = ((ptr_pos + length) / BLOCK_SIZE) + block_idx;
    int last_block_bytes = (ptr_pos + length) % BLOCK_SIZE;

    memcpy((void *)buf, (void *)&block[ptr_pos], BLOCK_SIZE - ptr_pos);
    buf += (BLOCK_SIZE - ptr_pos);
    block_idx++;

    while (block_idx < last_full_block_idx) {
        block_ptr = get_block_ptr_offset(inode_idx, block_idx);
        if (block_ptr == -1)
            return -1;

        read_blocks(DATA_START_ADDR + block_ptr, 1, (void *)block);
        memcpy((void *)buf, (void *)block, BLOCK_SIZE);
        buf += BLOCK_SIZE;
        block_idx++;
    }

    block_ptr = get_block_ptr_offset(inode_idx, block_idx);
    if (block_ptr == -1)
        return -1;

    read_blocks(DATA_START_ADDR + block_ptr, 1, (void *)block);
    memcpy((void *)buf, (void *)block, last_block_bytes);

    fd_t *tmp_fd = calloc(1, sizeof(fd_t));
    tmp_fd = &fd_table[fileID];
    tmp_fd->rw_ptr += length;
    memcpy((void *)&fd_table[fileID], (void *)tmp_fd, sizeof(fd_t));

    return length;
}

// Writes the amount of data stored in buf of size length into the file, starting from the file pointer
// Inputs:  int fileID  - Index of the file in the fd table
//          char *buf   - Buffer containing the data to be written
//          int length  - Number of bytes of the buffer
// Return:  int - 1 if successfully wrote to file, 0 otherwise
int sfs_fwrite(int fileID, const char *buf, int length) {
    if (fileID < 0 || fileID >= MAX_OPEN_FILES)
        return -1;

    // Make sure file is open
    if (fd_table[fileID].free == 1) {
        return -1;
    }

    // Make sure writing length bytes will not exceed max file size
    if (fd_table[fileID].rw_ptr + length > MAX_FILE_SIZE) {
        return -1;
    }

    int rw_ptr = fd_table[fileID].rw_ptr;
    int block_idx = rw_ptr / BLOCK_SIZE;
    int inode_idx = fd_table[fileID].inode_idx;
    int rw_pos = rw_ptr % BLOCK_SIZE;

    // Allocate more pointers if not enough space for length bytes
    if ((rw_ptr + length) > inode_table[inode_idx].size) {
        int room_left = BLOCK_SIZE - rw_pos;
        int blocks_needed = ceil((double)(length - room_left) / BLOCK_SIZE);

        if (allocate_empty_blocks(inode_idx, blocks_needed) == -1)
            return -1;
    }

    char block[BLOCK_SIZE];
    int block_ptr = get_block_ptr_offset(inode_idx, block_idx);

    if (block_ptr == -1) {
        return -1;
    }

    read_blocks(DATA_START_ADDR + block_ptr, 1, (void *)block);

    // Enough space left to write in current block
    if ((rw_pos + length) <= BLOCK_SIZE) {
        memcpy((void *)&block[rw_pos], (void *)buf, length);
        write_blocks(DATA_START_ADDR + block_ptr, 1, (void *)block);

        inode_t *tmp_inode = calloc(1, sizeof(inode_t));
        tmp_inode = &inode_table[inode_idx];
        tmp_inode->size = MAX(rw_ptr + length, inode_table[inode_idx].size);
        memcpy((void *)&inode_table[inode_idx], (void *)tmp_inode, sizeof(inode_t));

        fd_t *tmp_fd = calloc(1, sizeof(fd_t));
        tmp_fd = &fd_table[fileID];
        tmp_fd->rw_ptr += length;
        memcpy((void *)&fd_table[fileID], (void *)tmp_fd, sizeof(fd_t));

        return length;
    }

    int last_full_block_idx = ((rw_pos + length) / BLOCK_SIZE) + block_idx;
    int last_block_bytes = (rw_pos + length) % BLOCK_SIZE;

    memcpy((void *)&block[rw_pos], (void *)buf, BLOCK_SIZE - rw_pos);
    write_blocks(DATA_START_ADDR + block_ptr, 1, (void *)block);
    buf += (BLOCK_SIZE - rw_pos);
    block_idx++;

    while (block_idx < last_full_block_idx) {
        block_ptr = get_block_ptr_offset(inode_idx, block_idx);
        if (block_ptr == -1) {
            return -1;
        }

        read_blocks(DATA_START_ADDR + block_ptr, 1, (void *)block);
        memcpy((void *)block, (void *)buf, BLOCK_SIZE);
        write_blocks(DATA_START_ADDR + block_ptr, 1, (void *)block);

        buf += BLOCK_SIZE;
        block_idx++;
    }

    block_ptr = get_block_ptr_offset(inode_idx, block_idx);
    if (block_ptr == -1) {
        return -1;
    }

    read_blocks(DATA_START_ADDR + block_ptr, 1, (void *)block);
    memcpy((void *)block, (void *)buf, last_block_bytes);
    write_blocks(DATA_START_ADDR + block_ptr, 1, (void *)block);

    inode_t *tmp_inode = calloc(1, sizeof(inode_t));
    tmp_inode = &inode_table[inode_idx];
    tmp_inode->size = MAX(rw_ptr + length, inode_table[inode_idx].size);
    memcpy((void *)&inode_table[inode_idx], (void *)tmp_inode, sizeof(inode_t));

    fd_t *tmp_fd = calloc(1, sizeof(fd_t));
    tmp_fd = &fd_table[fileID];
    tmp_fd->rw_ptr += length;
    memcpy((void *)&fd_table[fileID], (void *)tmp_fd, sizeof(fd_t));

    return length;
}

// Move the rw pointer for a file to the specified location
// Inputs:  int fileID  - Index of the file in directory
//          int loc     - Desired index to place rw pointer
// Returns  int - 0 if successfully moved pointer, -1 otherwise
int sfs_fseek(int fileID, int loc) {
    if (fileID < 0 || fileID > MAX_OPEN_FILES - 1)
        return -1;

    // Check if file fd is open
    if (fd_table[fileID].free == 1)
        return -1;

    // Check if loc is within data range
    if (loc < 0)
        return -1;

    // Set rw_ptr to end of file if loc is bigger than the file size
    fd_t *tmp_fd = calloc(1, sizeof(fd_t));
    tmp_fd = &fd_table[fileID];
    tmp_fd->rw_ptr = MIN(inode_table[fd_table[fileID].inode_idx].size, loc);
    memcpy((void *)&fd_table[fileID], (void *)tmp_fd, sizeof(fd_t));

    return 0;
}

// Remove the file from the directory and release all data associated to it
// Inputs:  char *file  - Name of the file to remove
// Return:  int - 0 if successfully removed file, -1 otherwise
int sfs_remove(char *file) {
    int i;

    // Get file's inode
    int inode_idx = get_inode(file);

    if (inode_idx == -1)
        return -1;

    // If the file is open, close it
    for (i = 0; i < MAX_OPEN_FILES; i++)
        if (fd_table[i].inode_idx == inode_idx) {
            if (sfs_fclose(i) == -1)
                return -1;

            fd_t *tmp_fd = calloc(1, sizeof(fd_t));
            tmp_fd = &fd_table[i];
            tmp_fd->inode_idx = 1;
            memcpy((void *)&fd_table[i], (void *)tmp_fd, sizeof(fd_t));
            fd_table[i].inode_idx = -1;
            break;
        }

    // Clear file's inode
    int num_files = get_num_files();
    inode_t *tmp_inode = calloc(1, sizeof(inode_t));
    tmp_inode = &inode_table[ROOT_IDX];
    tmp_inode->size -= sizeof(dir_entry_t);
    memcpy((void *)&inode_table[ROOT_IDX], (void *)tmp_inode, sizeof(inode_t));

    int num_blocks = (ceil)((double)inode_table[inode_idx].size / BLOCK_SIZE);
    tmp_inode = &inode_table[inode_idx];
    tmp_inode->size = 0;
    tmp_inode->mode = 0;
    tmp_inode->linked = 0;
    memcpy((void *)&inode_table[inode_idx], (void *)tmp_inode, sizeof(inode_t));

    // Write inode to disk
    write_inode(inode_idx);

    // Clear data blocks associated to the file and free bitmap
    char *disk_bitmap;
    char bitmap_buffer[BLOCK_SIZE * BITMAP_SIZE];
    read_blocks(BITMAP_START_ADDR, BITMAP_SIZE, (void *)bitmap_buffer);
    disk_bitmap = (char *)bitmap_buffer;

    int ptr_idx;
    for (i = 0; i < (MIN(num_blocks, DATA_POINTERS)); i++) {
        ptr_idx = inode_table[inode_idx].data_ptrs[i];
        tmp_inode = &inode_table[inode_idx];
        tmp_inode->data_ptrs[i] = -1;
        memcpy((void *)&inode_table[inode_idx], (void *)tmp_inode, sizeof(inode_t));
        disk_bitmap[ptr_idx / BLOCK_SIZE]++;
        free_bitmap[ptr_idx] = 1;
    }

    if (num_blocks > DATA_POINTERS) {
        char ptr_buffer[BLOCK_SIZE];
        read_blocks(DATA_START_ADDR + inode_table[inode_idx].indirect_pointer, 1, (void *)ptr_buffer);
        int *ptrs = (int *)ptr_buffer;

        char free = 1;
        int num_ptrs = (num_blocks - DATA_POINTERS) / 4;
        for (i = 0; i < num_ptrs; i++) {
            memcpy((void *)&disk_bitmap[num_ptrs], (void *)&free, sizeof(char));
            free_bitmap[num_ptrs] = 1;
            ptrs++;
        }

        tmp_inode = &inode_table[inode_idx];
        tmp_inode->indirect_pointer = 0;
        memcpy((void *)&inode_table[inode_idx], (void *)tmp_inode, sizeof(inode_t));
    }

    // Write bitmap to disk
    write_blocks(BITMAP_START_ADDR, BITMAP_SIZE, (void *)bitmap_buffer);

    // Remove file from directory
    int start_file;
    int start_dir_block;

    inode_table[ROOT_IDX].size -= sizeof(dir_entry_t);
    for (i = 0; i < num_files; i++) {
        if (strcmp(root_dir[i].name, file) == 0) {
            start_file = i;
            start_dir_block = DIR_START_ADDR + i / FILES_PER_BLOCK;
            num_blocks = DIR_SIZE - i / FILES_PER_BLOCK + 1;
            memmove((void *)&root_dir[i], (void *)&root_dir[i + 1], sizeof(dir_entry_t) * (num_files - i));
            break;
        }
    }

    // Write files to disk; is a bit convoluted as sizeof(dir_entry_t) % BLOCK_SIZE != 0
    dir_entry_t *disk_dir1;
    dir_entry_t *disk_dir2;
    char buffer1[BLOCK_SIZE];
    char buffer2[BLOCK_SIZE];

    if (num_files > 0) {
        read_blocks(start_dir_block, 1, (void *)buffer1);
        disk_dir1 = (dir_entry_t *)buffer1;
    }

    for (i = 0; i < num_blocks; i++) {
        start_file = start_file % FILES_PER_BLOCK;
        memmove((void *)(disk_dir1 + start_file), (void *)(disk_dir1 + start_file + 1), sizeof(dir_entry_t) * (FILES_PER_BLOCK - start_file));
        start_file = 0;

        if ((num_blocks - i) != 1) {
            read_blocks(start_dir_block + i + 1, 1, (void *)buffer2);
            disk_dir2 = (dir_entry_t *)buffer2;
            memmove((void *)(disk_dir1 + FILES_PER_BLOCK), (void *)disk_dir2, sizeof(dir_entry_t));
            memmove((void *)disk_dir2, (void *)(disk_dir2 + 1), sizeof(dir_entry_t) * (FILES_PER_BLOCK - 1));
            disk_dir1 = disk_dir2;
        } else {
            write_blocks(start_dir_block + i + 1, 1, (void *)buffer2);
        }

        write_blocks(start_dir_block + i, 1, (void *)buffer1);
    }

    return 0;
}


// ====================
// | HELPER FUNCTIONS |
// ====================

// Get number of files in directory
int get_num_files() {
    return inode_table[ROOT_IDX].size / sizeof(dir_entry_t);
}

// Find inode with same path
int get_inode(char *path) {
    for (int i = 0; i < get_num_files(); i++) {
        if (strcmp(root_dir[i].name, path) == 0) {
            return root_dir[i].inode_idx;
        }
    }
    return -1;
}

// Read and write to disk one block at a time
void write_inode(int inode_idx) {
    inode_t *disk_inode;
    char buffer[BLOCK_SIZE];

    int block_num = INODE_START_ADDR + inode_idx / INODES_PER_BLOCK;
    int idx = inode_idx % INODES_PER_BLOCK;

    read_blocks(block_num, 1, (void *)buffer);
    disk_inode = (inode_t *)buffer;
    memcpy((void *)&disk_inode[idx], (void *)&inode_table[inode_idx], sizeof(inode_t));
    write_blocks(block_num, 1, (void *)buffer);
}

// Read and write to disk one block at a time
void write_dir_entry(int dir_idx) {
    dir_entry_t *disk_file;
    char buffer[BLOCK_SIZE];

    int block_num = DIR_START_ADDR + dir_idx / FILES_PER_BLOCK;
    int idx = dir_idx % FILES_PER_BLOCK;
    read_blocks(block_num, 1, (void *)buffer);
    disk_file = (dir_entry_t *)buffer;
    memcpy((void *)&disk_file[idx], (void *)&root_dir[dir_idx], sizeof(dir_entry_t));
    write_blocks(block_num, 1, (void *)buffer);
}

// Read and write to disk one block at a time
void write_bitmap(int bit_idx) {
    char *disk_bitmap;
    char buffer[BLOCK_SIZE];

    int block_num = BITMAP_START_ADDR + bit_idx / BLOCK_SIZE;
    int idx = bit_idx % BLOCK_SIZE;
    read_blocks(block_num, 1, (void *)buffer);
    disk_bitmap = (char *)buffer;
    memcpy((void *)&disk_bitmap[idx], (void *)&free_bitmap[bit_idx], sizeof(char));
    write_blocks(block_num, 1, (void *)buffer);
}

// linear search to find empty file data blocks from bitmap and allocate atomically
// returns -1 if not enough room left on disk to allocate all blocks, else returns number of blocks allocated
// only used by sfs_fwrite
int allocate_empty_blocks(int inode_idx, int num_blocks_needed) {
    int num = num_blocks_needed;
    int start_blocks = ceil((double)inode_table[inode_idx].size / BLOCK_SIZE);
    int inode_ptr_block = 0;

    if (inode_table[inode_idx].indirect_pointer <= 0 && (start_blocks + num_blocks_needed) > DATA_POINTERS) {
        inode_ptr_block = 1;
        num++;
    }

    int i, j;
    int flag = 0;
    int start = 0;
    int ptrs[num];
    // Find all empty blocks
    for (i = 0; i < num; i++) {
        for (j = start; j < DATA_BLOCKS; j++) {
            if (free_bitmap[j] == 1) {
                ptrs[i] = j;
                start = j + 1;
                break;
            }
        }
        if (j == DATA_BLOCKS)
            flag = 1;
    }

    if (flag == 1) {
        printf("SFS has reached maximum data capacity! Please delete some data before writing more.\n");
        return -1;
    }

    if (inode_ptr_block == 1) {
        inode_t *tmp_inode = calloc(1, sizeof(inode_t));
        tmp_inode = &inode_table[inode_idx];
        tmp_inode->indirect_pointer = ptrs[num - 1];
        memcpy((void *)&inode_table[inode_idx], (void *)tmp_inode, sizeof(inode_t));
    }

    // Mark used blocks in bitmap and write entire bitmap to disk
    for (i = 0; i < num; i++)
        memset((void *)&free_bitmap[ptrs[i]], 0, sizeof(char));

    write_blocks(BITMAP_START_ADDR, BITMAP_SIZE, (void *)&free_bitmap);

    int ptr_idx;
    int cur_block = start_blocks;
    // Allocate empty blocks to the inode pointers and mark as used in the bitmap
    for (i = 0; i < num_blocks_needed; i++) {
        if (cur_block < DATA_POINTERS) {
            ptr_idx = cur_block - 1;
            inode_t *tmp_inode = calloc(1, sizeof(inode_t));
            tmp_inode = &inode_table[inode_idx];
            tmp_inode->data_ptrs[ptr_idx] = ptrs[i];
            memcpy((void *)&inode_table[inode_idx], (void *)tmp_inode, sizeof(inode_t));
            ptr_idx++;
        } else {
            ptr_idx = cur_block - DATA_POINTERS - 1;
            char buffer[BLOCK_SIZE];
            int addr = DATA_START_ADDR + inode_table[inode_idx].indirect_pointer;
            read_blocks(addr, 1, (void *)buffer);
            int *indrct_ptrs = (int *)buffer;
            indrct_ptrs += ptr_idx;
            memcpy((void *)indrct_ptrs, (void *)&ptrs[i], sizeof(int) * (num_blocks_needed - i));
            write_blocks(DATA_START_ADDR + inode_table[inode_idx].indirect_pointer, 1, (void *)buffer);
            break;
        }
        cur_block++;
    }

    // Write inodes to disk
    write_inode(inode_idx);

    return num;
}

// Returns pointer to a block given the number block of a file
int get_block_ptr_offset(int inode_idx, int block_num) {
    int ptr_num = -1;
    if (block_num <= DATA_POINTERS)
        ptr_num = inode_table[inode_idx].data_ptrs[block_num - 1];
    else {
        int block_offset = block_num - DATA_POINTERS + 1;
        char buffer[BLOCK_SIZE];
        read_blocks(DATA_START_ADDR + inode_table[inode_idx].indirect_pointer, 1, (void *)buffer);
        int i;
        int *ptrs = (int *)buffer;
        for (i = 0; i < block_offset; i++)
            ptrs++;

        ptr_num = *ptrs;
    }

    return ptr_num;
}
