#include "fs/buf.h"
#include "fs/fs.h"
#include "fs/bitmap.h"
#include "lib/print.h"

extern super_block_t sb;

// search and set bit
// 该函数用于在指定的块位图中搜索一个空闲的块 & 将其标记为已分配 & 返回该块的块号。
static uint32 bitmap_search_and_set(uint32 bitmap_block)
{
    // 从磁盘读取指定的块位图到缓冲区
    buf_t* bp = buf_read(bitmap_block);
    uint32 m, block_num;

    // 遍历整个块位图的每一位（每个 bit 对应一个块）
    for (uint32 bi = 0; bi < sb.block_size * 8; ++bi)  // sb.block_size * 8 是位图的总 bit 数
    {
        m = 1 << (bi % 8);  // 计算当前 bit 对应的掩码（每个字节有 8 个 bit）

        // 如果当前位为 0，表示该块空闲
        if ((bp->data[bi / 8] & m) == 0)   // 用bi / 8对应字节去与
        {
            // 将当前 bit 设置为 1，表示该块已分配
            bp->data[bi / 8] |= m;

            // 将更新后的位图写回磁盘
            buf_write(bp);
            buf_release(bp);

            // 分配的块号（当前位偏移量 + 位图块的起始块号 + 1）
            block_num = bi + bitmap_block + 1;

            // 从磁盘读取分配的块
            buf_t *buf = buf_read(block_num);

            // 将更新后的块写回磁盘
            buf_write(buf);
            buf_release(buf);

            // 返回分配的块号
            return block_num;
        }
    }

    // 如果没有找到空闲块，抛出异常
    panic("bitmap_search_and_set: out of blocks");
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
    // 从磁盘读取指定的块位图到缓冲区
    buf_t* buf = buf_read(bitmap_block);

    // 计算给定块号对应的位图中的偏移量
    num -= bitmap_block + 1;

    // 计算该块在位图中的位置
    uint8 m = 1 << (num % 8);

    // 将对应的位设置为 0，表示该块已释放
    buf->data[num / 8] &= ~m;

    // 将更新后的位图写回磁盘
    buf_write(buf);
    buf_release(buf);
}

// bitmap_free_block
// 该函数用于释放一个数据块，更新数据块位图。
// 输入参数：block_num - 需要释放的块号
void bitmap_free_block(uint32 block_num)
{
    // 在数据块位图中释放指定的块
    bitmap_unset(sb.data_bitmap_start, block_num);
}

// bitmap_alloc_inode
// 该函数用于分配一个 inode（在 inode 位图中），并返回该 inode 的编号。
// 返回值：分配的 inode 编号
uint32 bitmap_alloc_inode()
{
    // 在 inode 位图中查找并分配一个空闲 inode
    return (uint32)bitmap_search_and_set(sb.inode_bitmap_start);
}

// bitmap_free_inode
// 该函数用于释放一个 inode，更新 inode 位图。
// 输入参数：inode_num - 需要释放的 inode 编号
void bitmap_free_inode(uint32 inode_num)
{
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
            printf("Bit %d is used\n", i);

        // else printf("Bit %d is not used\n", i);
    }

    buf_release(buf);
}