#include "fs/buf.h"
#include "fs/fs.h"
#include "fs/bitmap.h"
#include "lib/print.h"
#include "lib/str.h"

extern super_block_t sb;

// search and set bit
// 该函数用于在指定的块位图中搜索一个空闲的块 & 将其标记为已分配 & 返回该块的块号。
static uint32 bitmap_search_and_set(uint32 bitmap_block)
{
    buf_t* bp = buf_read(bitmap_block);
    uint32 m, block_num;

    for (uint32 bi = 0; bi < sb.block_size * 8; ++bi) 
    {
        m = 1 << (bi % 8);  

        if ((bp->data[bi / 8] & m) == 0) 
        {
            bp->data[bi / 8] |= m;

            buf_write(bp);
            buf_release(bp);

            block_num = bi + bitmap_block;

            buf_t *buf = buf_read(block_num);
            memset(buf->data, 0, BLOCK_SIZE);
            buf_release(buf);

            return block_num;
        }
    }

    buf_release(bp);
    return -1;
}

// bitmap_alloc_block
// 该函数用于分配一个数据块（在数据块位图中），并将该块清零。
// 返回分配的块号，如果没有空闲块，返回 -1。
uint32 bitmap_alloc_block()
{
    // 在数据块位图中查找并分配一个空闲块
    return bitmap_search_and_set(sb.data_bitmap_start);
}

// unset bit
// 该函数用于在指定的块位图中释放指定的块，设置对应的位为 0（表示空闲）。
static void bitmap_unset(uint32 bitmap_block, uint32 num)
{
    buf_t* buf = buf_read(bitmap_block);

    num -= bitmap_block;
    uint8 m = 1 << (num % 8);
    buf->data[num / 8] &= ~m;

    buf_write(buf);
    buf_release(buf);
}

// bitmap_free_block
void bitmap_free_block(uint32 block_num)
{
    bitmap_unset(sb.data_bitmap_start, block_num);
}

// bitmap_alloc_inode
uint16 bitmap_alloc_inode()
{
    return (uint16)bitmap_search_and_set(sb.inode_bitmap_start);
}

// bitmap_free_inode
void bitmap_free_inode(uint16 inode_num)
{
    // CHECK!
    // 在 inode 位图中释放指定的 inode
    bitmap_unset(sb.inode_bitmap_start, inode_num);
}

// 打印所有已经分配出去的 bit 序号（序号从 0 开始），用于调试
// 输入参数：bitmap_block_num - 要打印的位图块号
void bitmap_print(uint32 bitmap_block_num)
{
    // 从磁盘读取指定的块位图到缓冲区
    buf_t* buf = buf_read(bitmap_block_num);

    // 打印该位图中所有位
    printf("bits in bitmap %d:\n", bitmap_block_num);
    for (uint32 i = 0; i < sb.block_size * 8; i++) {
        uint8 m = 1 << (i % 8);

        // 如果当前位为 1，表示该块已分配
        if ((buf->data[i / 8] & m) != 0) 
            printf("Bit %d is alloced\n", i);
            
    }
    buf_release(buf);
}