#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <assert.h>

// disk layout: [ super block | inode bitmap | inode blocks | data bitmap | data blocks ]

#define FS_MAGIC 0x12345678

typedef struct super_block {
    unsigned int magic;
    unsigned int block_size;

    unsigned int inode_bitmap_start;
    unsigned int inode_start;
    unsigned int data_bitmap_start;
    unsigned int data_start;

    unsigned int inode_blocks;
    unsigned int data_blocks;
    unsigned int total_blocks;

} super_block_t;

typedef struct inode_disk {
    short type;
    short major;
    short minor;
    short nlink;
    unsigned int size;
    unsigned int addrs[13];
} inode_disk_t;

// 常量定义 
#define BLOCK_SIZE       1024
#define N_DATA_BLOCK     8192 // 1个block的bitmap管理的极限
#define N_INODE_BLOCK    128  // 支持1024个文件
#define N_BLOCK          (N_DATA_BLOCK + N_INODE_BLOCK + 3)  // 五个部分组合起来
#define INODE_PER_BLOCK  (BLOCK_SIZE / sizeof(inode_disk_t)) // 每个block里的inode数量
#define N_INODE          (N_INODE_BLOCK * INODE_PER_BLOCK)   // inode总数

int fsfd;
super_block_t sb;

unsigned short xshort(unsigned short x)
{
    unsigned short y;
    unsigned char* a = (unsigned char*)&y;
    a[0] = x;
    a[1] = x >> 8;
    return y;
}

unsigned int xint(unsigned int x)
{
    unsigned int y;
    unsigned char* a = (unsigned char*)&y;
    a[0] = x;
    a[1] = x >> 8;
    a[2] = x >> 16;
    a[3] = x >> 24;
    return y;
}

void block_write(unsigned int block_num, void* buf)
{
    if(lseek(fsfd, BLOCK_SIZE * block_num, 0) != BLOCK_SIZE * block_num) {
        perror("lsek");
        exit(1);
    }
    if(write(fsfd, buf, BLOCK_SIZE) != BLOCK_SIZE) {
        perror("write");
        exit(1);
    }
}

void block_read(unsigned int block_num, void* buf)
{
    if(lseek(fsfd, BLOCK_SIZE * block_num, 0) != BLOCK_SIZE * block_num) {
        perror("lsek");
        exit(1);
    }
    if(write(fsfd, buf, BLOCK_SIZE) != BLOCK_SIZE) {
        perror("read");
        exit(1);
    }
}

int main(int argc, char* argv[])
{
    assert(BLOCK_SIZE % sizeof(inode_disk_t) == 0);
    
    // 创建磁盘文件
    fsfd = open(argv[1], O_RDWR | O_CREAT | O_TRUNC, 0666);
    if(fsfd < 0) {
        perror(argv[1]);
        exit(1);
    }

    // super block 填充
    sb.magic = FS_MAGIC;
    sb.block_size = xint(BLOCK_SIZE);
    sb.inode_blocks = xint(N_INODE_BLOCK);
    sb.data_blocks = xint(N_DATA_BLOCK);
    sb.total_blocks = xint(N_BLOCK);
    sb.inode_bitmap_start = xint(1);
    sb.inode_start = xint(1 + 1);
    sb.data_bitmap_start = xint(1 + 1 + N_INODE_BLOCK);
    sb.data_start = xint(1 + 1 + N_INODE_BLOCK + 1);

    // 缓冲区准备
    char buf[BLOCK_SIZE];
    memset(buf, 0, sizeof(buf));

    // 一个全0的磁盘映像
    for(int i = 0; i < N_BLOCK; i++)
        block_write(i, buf);

    // 填写 super block
    memmove(buf, &sb, sizeof(sb));
    block_write(0, buf);

    return 0;
}