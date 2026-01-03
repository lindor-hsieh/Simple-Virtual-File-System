#include "inode.h"
#include "bitmap.h"
#include <string.h>
#include <stdio.h>

int find_free_inode() 
{
    for(int i=0; i<MAX_FILES; i++) 
        if(!inode_table[i].is_used) return i;
    return -1;
}

// 根據檔名和目錄 ID 尋找 Inode
int find_inode_by_name(char *name, int dir_id) 
{
    for(int i=0; i<MAX_FILES; i++) 
    {
        // 必須是被使用的 + 父目錄 ID 符合 + 檔名相同才是我們要找的
        if(inode_table[i].is_used && inode_table[i].parent_id == dir_id &&
           strcmp(inode_table[i].name, name)==0) return i;
    }
    return -1;
}

// Check Permission
int check_permission(int inode_idx, int mode) 
{
    // if mode 1 是寫入檢查，then if permission < 4 代表read only
    if(mode == 1 && inode_table[inode_idx].permission < 4) return 0;
    return 1;
}

void recursive_delete(int inode_idx) 
{
    if(!inode_table[inode_idx].is_used) return;

    if(inode_table[inode_idx].is_dir) 
    {
        // 如果是目錄，先遞迴刪除所有子檔案
        for(int i=0; i<MAX_FILES; i++) 
        {
            if(inode_table[i].is_used && inode_table[i].parent_id == inode_idx)
                recursive_delete(i);
        }
    } 
    else 
    {
        // 如果是檔案，release所佔用的 Block
        int blks = (inode_table[inode_idx].size + BLOCK_SIZE - 1)/BLOCK_SIZE;
        for(int b=0; b<blks; b++) 
            free_block(inode_table[inode_idx].blocks[b]);
    }
    
    // 最後release Inode
    inode_table[inode_idx].is_used = 0;
    sb->used_inodes--;
}