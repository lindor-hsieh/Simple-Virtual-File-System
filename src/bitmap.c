#include "bitmap.h"
#include <stdio.h>

// Bitwise Operation
// 1 byte = 8 bits,所以一個 char 變數可以紀錄 8 個 Blocks 的狀態
// i/8 (Index): 算出第 i 個 Block 位於 Bitmap 的第幾格
// i%8 (Offset): 算出該 Block 位於該 Byte 裡面的第幾個 Bit (0-7)
void set_bit(int i) 
{ 
    // 1 << (i%8): 將 1 向左位移,作出只有該 bit 為 1 的Mask
    // |=: 將原本跟Mask做 OR,強制設為 1
    block_bitmap[i/8] |= (1 << (i%8)); 
}

void clear_bit(int i) 
{ 
    block_bitmap[i/8] &= ~(1 << (i%8)); 
}

int get_bit(int i) 
{ 
    // &: 檢查該位置是否為 1
    return block_bitmap[i/8] & (1 << (i%8)); 
}

// 尋找第一個可用的Block
int find_free_block() 
{
    for(int i=0; i<sb->total_blocks; i++) 
    {
        if(get_bit(i) == 0) 
        {
            set_bit(i);
            sb->used_blocks++;
            return i;           // 讓 Inode 去紀錄
        }
    }
    return -1;
}

void free_block(int block_id) 
{
    // 防呆
    if(block_id < 0 || block_id >= sb->total_blocks) return;

    if(get_bit(block_id)) 
    { 
        clear_bit(block_id);
        sb->used_blocks--;
    }
}