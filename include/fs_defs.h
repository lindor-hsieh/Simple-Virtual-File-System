#ifndef FS_DEFS_H
#define FS_DEFS_H

#include <time.h>
#include <stdint.h>

#define MAX_CMD_LEN 256
#define MAX_FILENAME 32
#define MAX_FILES 100 // Inode 數量
#define BLOCK_SIZE 1024
#define MAX_BLOCKS_PER_FILE 128 // 最大檔案大小 128KB

// Superblock,global info.
typedef struct 
{
    int total_size;
    int block_size;
    int total_inodes;
    int used_inodes;
    int total_blocks;
    int used_blocks;
    char password[32];
} Superblock;

// Inode (index的概念)
typedef struct 
{
    int id; // Inode 編號
    int is_used;
    int is_dir;
    char name[MAX_FILENAME];
    int parent_id; // 目錄結構
    int size;
    int blocks[MAX_BLOCKS_PER_FILE]; // 儲存data block的索引列表
    time_t created_at; // 建立時間 (not used)
    int permission; // 權限設定(like chmod)
} Inode;

// DiskBlock,data block
typedef struct 
{
    char data[BLOCK_SIZE]; 
} DiskBlock;

#endif