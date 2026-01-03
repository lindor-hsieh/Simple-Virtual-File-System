#ifndef FS_H
#define FS_H
#include "fs_defs.h"
#include <stdint.h>

extern Superblock *sb;
extern Inode *inode_table;
extern DiskBlock *data_blocks;
extern uint8_t *block_bitmap;

extern int current_dir_id;
extern char current_path[256];

void init_fs(int size, int load_from_file); // create or read
void save_fs(const char *filename);         // 存檔 (Dump)
void defrag_system();                       // 磁碟重組

#endif