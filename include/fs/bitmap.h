#ifndef __BITMAP_H__
#define __BITMAP_H__

uint32 bitmap_alloc_block();
uint32 bitmap_alloc_inode();
void   bitmap_free_block(uint32 block_num);
void   bitmap_free_inode(uint32 inode_num);
void   bitmap_print(uint32 bitmap_block_num);

#endif