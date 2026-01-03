#ifndef BITMAP_H
#define BITMAP_H
#include "fs_defs.h"

extern Superblock *sb;
extern uint8_t *block_bitmap; 

// Bit Operation
void set_bit(int i); // which is occupied
void clear_bit(int i); // which is released
int get_bit(int i);
int find_free_block(); 
void free_block(int block_id);

#endif