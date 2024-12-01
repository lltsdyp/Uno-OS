# Uno-OS

## 简介
Uno-OS是一个基于MIT的xv6实验开发的，RISCV架构的简化版UNIX操作系统。

## 文件系统初始化
在初始化文件系统时，我们主要做以下两件事：      
* 初始化缓冲区
* 从磁盘中读取superblock块，并将信息写到内存中的superblock结构中，在这里我们会检查sb的magic值和size是否合法
```c
void fs_init()
{
    buf_init();

    buf_t* buf = buf_read(SB_BLOCK_NUM);
    memcpy(&sb, buf->data, sizeof(sb));
    buf_release(buf);

    // 检查超级块的magic值是否正确，确保文件系统没有损坏
    assert(sb.magic == FS_MAGIC, "fs_init: invalid super block magic number");
    assert(sb.block_size == BLOCK_SIZE, "fs_init: invalid super block size");

    sb_print();
}
```

### bitmap 的管理
这里主要关注`bitmap_search_and_set`和`bitmap_unset`两个函数的实现。         
第一个函数`bitmap_search_and_set`：
* 将对应bitmap读取到缓冲区后，就开始遍历bitmap的每一位以找到一个空闲块，
* 找到空闲块之后，设置该块对应的位为1表示已分配
* 然后再从磁盘中读取分配的块，并返回对应的块号
* 如果没有找到空闲的块就报错
```c
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

            // 分配的块号（当前位偏移量 + 位图块的起始块号）
            block_num = bi + bitmap_block;

            buf_t *buf = buf_read(block_num);
            memset(bp->data, 0, BLOCK_SIZE);
            buf_release(buf);

            return block_num;
        }
    }

    // 如果没有找到空闲块，抛出异常
    panic("bitmap_search_and_set: out of blocks");
    return -1;
}
```
函数`bitmap_unset`则是相反的操作：
* 先用`num`减去指定的bitmap起始块号，获得要释放的块的偏移量，对应前面的`block_num = bi + bitmap_block;`
* 然后将该位设置为0表示块未分配即可
```c
// unset bit
// 该函数用于在指定的块位图中释放指定的块，设置对应的位为 0（表示空闲）。
static void bitmap_unset(uint32 bitmap_block, uint32 num)
{
    // 从磁盘读取指定的块位图到缓冲区
    buf_t* buf = buf_read(bitmap_block);

    // 计算给定块号对应的位图中的偏移量
    num -= bitmap_block;

    // 计算该块在位图中的位置
    uint8 m = 1 << (num % 8);

    // 将对应的位设置为 0，表示该块已释放
    buf->data[num / 8] &= ~m;

    // 将更新后的位图写回磁盘
    buf_write(buf);
    buf_release(buf);
}
```
下面四个函数是对前面两个函数的封装，以便能区分是对data_block还是inode_block进行操作。
```c
// bitmap_alloc_block
uint32 bitmap_alloc_block()
{
    // 在数据块位图中查找并分配一个空闲块
    return bitmap_search_and_set(sb.data_bitmap_start);
}

// bitmap_free_block
void bitmap_free_block(uint32 block_num)
{
    // 在数据块位图中释放指定的块
    bitmap_unset(sb.data_bitmap_start, block_num);
}

// bitmap_alloc_inode
uint32 bitmap_alloc_inode()
{
    // 在 inode 位图中查找并分配一个空闲 inode
    return (uint32)bitmap_search_and_set(sb.inode_bitmap_start);
}

// bitmap_free_inode
void bitmap_free_inode(uint32 inode_num)
{
    // 在 inode 位图中释放指定的 inode
    bitmap_unset(sb.inode_bitmap_start, inode_num);
}
```
函数`bitmap_print`用于调试，他会从磁盘中读取指定的bitmap，然后遍历每个位，输出bitmap中每个block的分配情况。
```c
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
```