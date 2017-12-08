#ifndef _INCLUDE_SFS_API_H_
#define _INCLUDE_SFS_API_H_

#include <stdint.h>

#define DATA_POINTERS 12
#define MAX_EXT_NAME 3
#define MAX_FILE_NAME 16
#define MAX_FILE_LENGTH 20

// ===========
// | STRUCTS |
// ===========

// Superblock
// magic                Magic number
// block_size           Total size of superblock
// fs_size
// inode_table_len      Number of inodes in table
// root_dir_inode       Inode of the root directory
typedef struct superblock_t {
    int magic;
    int block_size;
    int fs_size;
    int inode_table_len;
    int root_dir_inode;
} superblock_t;

// Inode
// mode                 File permissions
// linked               Whether the inode is being used
// size                 Total size of used data blocks
// data_ptrs            Points to used data blocks
// indirect_pointer     Points to a data block that points to other data blocks
typedef struct inode_t {
    int mode;
    int linked;
    // int uid;
    // int gid;
    int size;
    int data_ptrs[DATA_POINTERS];
    int indirect_pointer;
} inode_t;

// File descriptor
// inode_index  Which inode this entry describes
// inode        Pointer towards the inode in the inode table
// free         Whether the fd is already in use (not free)
// rwptr        Where in the file to start
typedef struct fd_t {
    int inode_idx;
    inode_t* inode;
    int free;
    int rw_ptr;
} fd_t;

// Directory entry
// inode_idx    Represents the inode number of the entry
// name         Represents the name of the entry
typedef struct dir_entry_t {
    int inode_idx;
    char name[MAX_FILE_LENGTH];
} dir_entry_t;

// ==================
// | MAIN FUNCTIONS |
// ==================

void mksfs(int fresh);
int sfs_getnextfilename(char *fname);
int sfs_getfilesize(const char* path);
int sfs_fopen(char *name);
int sfs_fclose(int fileID);
int sfs_fread(int fileID, char *buf, int length);
int sfs_fwrite(int fileID, const char *buf, int length);
int sfs_fseek(int fileID, int loc);
int sfs_remove(char *file);

// ====================
// | HELPER FUNCTIONS |
// ====================

int get_num_files();
int get_inode(char *path);
void write_inode(int inode_idx);
void write_dir_entry(int dir_idx);
void write_bitmap(int bit_idx);
int allocate_empty_blocks(int inode_idx, int num_blocks_needed);
int get_block_ptr_offset(int inode_idx, int block_num);

#endif //_INCLUDE_SFS_API_H_
