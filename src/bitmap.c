#include "bitmap.h"
#include <stdio.h>

// i/8 是指找到是第幾個 byte，i%8 找到是該 byte 裡的第幾個 bit
void set_bit(int i) 
{ 
    block_bitmap[i/8] |= (1 << (i%8)); 
}

void clear_bit(int i) 
{ 
    block_bitmap[i/8] &= ~(1 << (i%8)); 
}

int get_bit(int i) 
{ 
    return block_bitmap[i/8] & (1 << (i%8)); 
}

int find_free_block() 
{
    for(int i=0; i<sb->total_blocks; i++) 
    {
        if(get_bit(i) == 0) 
        {
            set_bit(i);
            sb->used_blocks++; // used_blocks +1
            return i;         // return Block ID
        }
    }
    return -1;
}

void free_block(int block_id) 
{
    if(block_id < 0 || block_id >= sb->total_blocks) return;
    if(get_bit(block_id)) 
    { 
        clear_bit(block_id);
        sb->used_blocks--;
    }
}