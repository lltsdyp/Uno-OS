#include "fs/buf.h"
#include "fs/fs.h"
#include "fs/bitmap.h"
#include "lib/print.h"

extern super_block_t sb;

// search and set bit
static uint32 bitmap_search_and_set(uint32 bitmap_block)
{

}

// unset bit
static void bitmap_unset(uint32 bitmap_block, uint32 num)
{

}

uint32 bitmap_alloc_block()
{

}

void bitmap_free_block(uint32 block_num)
{

}

uint32 bitmap_alloc_inode()
{

}

void bitmap_free_inode(uint32 inode_num)
{

}

// 打印所有已经分配出去的bit序号(序号从0开始)
// for debug
void bitmap_print(uint32 bitmap_block_num)
{

}