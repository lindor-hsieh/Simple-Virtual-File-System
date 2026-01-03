#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "fs.h"
#include "security.h"
#include "bitmap.h"
#include "utils.h"

Superblock *sb;
Inode *inode_table;
DiskBlock *data_blocks;
uint8_t *block_bitmap;
int current_dir_id = 0;
char current_path[256] = "/";

// Initialize
void init_fs(int size, int load_from_file) 
{
    if (load_from_file) 
    {
        FILE *fp = fopen("my_fs.dump", "rb");
        if (!fp) 
        { 
            printf(C_ERR "Error: Cannot open dump.\n" C_RESET); exit(1); 
        }
        
        // Step 1: read Superblock
        sb = (Superblock*)malloc(sizeof(Superblock));
        fread(sb, sizeof(Superblock), 1, fp);

        // Step 2: Password
        if (!check_password(sb->password)) 
        { 
            fclose(fp); free(sb); exit(1); 
        }

        // Step 3: read Inode Table (encrypted data now)
        inode_table = (Inode*)malloc(sizeof(Inode) * MAX_FILES);
        fread(inode_table, sizeof(Inode), MAX_FILES, fp);
        
        // Step 4: read Data Blocks
        data_blocks = (DiskBlock*)malloc(sizeof(DiskBlock) * sb->total_blocks);
        fread(data_blocks, sizeof(DiskBlock), sb->total_blocks, fp);

        // Step 5: read Bitmap
        int b_size = (sb->total_blocks + 7) / 8;
        block_bitmap = (uint8_t*)malloc(b_size);
        fread(block_bitmap, 1, b_size, fp);
        fclose(fp);

        // Step 6: decrypted data
        if (strlen(sb->password)>0) 
        {
            xor_cipher(inode_table, sizeof(Inode)*MAX_FILES, sb->password);
            xor_cipher(data_blocks, sizeof(DiskBlock)*sb->total_blocks, sb->password);
        }
        printf(C_OK "FS Loaded.\n" C_RESET);

    } 
    else 
    {
        // Create Mode
        int meta = sizeof(Superblock) + (sizeof(Inode)*MAX_FILES);
        int num_blocks = (size - meta) / BLOCK_SIZE; // 計算可用的 Block 數量
        if(num_blocks <= 0) 
        { 
            printf(C_ERR "Size too small.\n" C_RESET); exit(1); 
        }

        // 分配記憶體
        sb = (Superblock*)malloc(sizeof(Superblock));
        inode_table = (Inode*)malloc(sizeof(Inode)*MAX_FILES);
        data_blocks = (DiskBlock*)malloc(sizeof(DiskBlock)*num_blocks);
        block_bitmap = (uint8_t*)calloc((num_blocks+7)/8, 1);

        // Initialize Superblock
        sb->total_size = size; sb->block_size = BLOCK_SIZE;
        sb->total_inodes = MAX_FILES; sb->used_inodes = 1;
        sb->total_blocks = num_blocks; sb->used_blocks = 0;
        
        set_new_password(sb->password, 32);

        // Initialize Inode
        for(int i=0; i<MAX_FILES; i++) inode_table[i].is_used=0;
        
        // Create Root 
        inode_table[0].is_used=1; inode_table[0].is_dir=1;
        inode_table[0].permission=7; strcpy(inode_table[0].name, "root");
        inode_table[0].parent_id=0;
        
        printf(C_OK "Partition created.\n" C_RESET);
    }
}

// 存檔成dump
void save_fs(const char *filename) 
{
    FILE *fp = fopen(filename, "wb");
    if(!fp) return;
    
    // Step 1: 寫入 Superblock
    fwrite(sb, sizeof(Superblock), 1, fp);

    // Step 2: Encrypt
    if(strlen(sb->password)>0) 
    {
        xor_cipher(inode_table, sizeof(Inode)*MAX_FILES, sb->password);
        xor_cipher(data_blocks, sizeof(DiskBlock)*sb->total_blocks, sb->password);
    }

    // Step 3: 寫入加密後的資料
    fwrite(inode_table, sizeof(Inode), MAX_FILES, fp);
    fwrite(data_blocks, sizeof(DiskBlock), sb->total_blocks, fp);
    int b_size = (sb->total_blocks + 7) / 8;
    fwrite(block_bitmap, 1, b_size, fp);

    // Step 4: Decrypt
    if(strlen(sb->password)>0) 
    {
        xor_cipher(inode_table, sizeof(Inode)*MAX_FILES, sb->password);
        xor_cipher(data_blocks, sizeof(DiskBlock)*sb->total_blocks, sb->password);
    }
    fclose(fp);
}

// 磁碟重組 (Defrag),將分散的 Used Blocks 全部搬移到陣列的前端
void defrag_system() 
{
    printf("Defragging...\n");
    int w_ptr = 0; // 寫入指標，指向新的緊湊空間
    
    // 建立新的暫存區
    DiskBlock *new_blks = malloc(sizeof(DiskBlock)*sb->total_blocks);
    uint8_t *new_map = calloc((sb->total_blocks+7)/8, 1);
    int used_cnt = 0;

    for(int i=0; i<MAX_FILES; i++) 
    {
        if(inode_table[i].is_used && !inode_table[i].is_dir && inode_table[i].size > 0) 
        {
            int needs = (inode_table[i].size + BLOCK_SIZE - 1)/BLOCK_SIZE;
            for(int b=0; b<needs; b++) 
            {
                int old = inode_table[i].blocks[b];
                // copy資料到新位置
                memcpy(new_blks[w_ptr].data, data_blocks[old].data, BLOCK_SIZE);
                // renew Inode 指標
                inode_table[i].blocks[b] = w_ptr;
                // renew 新 Bitmap
                new_map[w_ptr/8] |= (1<<(w_ptr%8));
                w_ptr++; used_cnt++;
            }
        }
    }
    free(data_blocks); free(block_bitmap);
    data_blocks = new_blks; block_bitmap = new_map;
    sb->used_blocks = used_cnt;
    printf(C_OK "Defrag Done.\n" C_RESET);
}