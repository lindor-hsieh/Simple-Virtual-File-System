#ifndef INODE_H
#define INODE_H
#include "fs_defs.h"

extern Inode *inode_table;
extern Superblock *sb;

// Inode Operation
int find_free_inode(); 
int find_inode_by_name(char *name, int dir_id); // 在指定目錄下找檔名
int check_permission(int inode_idx, int mode); 
void recursive_delete(int inode_idx); // 遞迴刪除 (針對目錄,for rm -r)

#endif