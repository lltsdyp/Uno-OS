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

// inode 64 byte
typedef struct inode_disk {
    short type;
    short major;
    short minor;
    short nlink;
    unsigned int size;
    unsigned int addrs[13];
} inode_disk_t;

// direntory entry 32 byte
typedef struct dirent {
    unsigned short inode_num;
    char name[30];
} dirent_t;

// 文件类型
#define FT_UNUSED 0
#define FT_DIR    1
#define FT_FILE   2
#define FT_DEVICE 3 

// 常量定义 
#define BLOCK_SIZE       1024
#define N_DATA_BLOCK     8192 // 1个block的bitmap管理的极限
#define N_INODE_BLOCK    128  // 支持1024个文件
#define N_BLOCK          (N_DATA_BLOCK + N_INODE_BLOCK + 3)  // 五个部分组合起来
#define INODE_PER_BLOCK  (BLOCK_SIZE / sizeof(inode_disk_t)) // 每个block里的inode数量
#define N_INODE          (N_INODE_BLOCK * INODE_PER_BLOCK)   // inode总数

// 确定inode所在的inode block序号
#define INODE_LOCATE_BLOCK(inum, sb)  ((inum) / INODE_PER_BLOCK + sb.inode_start)

int fsfd;
super_block_t sb;

// 大小端转换
unsigned short xshort(unsigned short x)
{
    unsigned short y;
    unsigned char* a = (unsigned char*)&y;
    a[0] = x;
    a[1] = x >> 8;
    return y;
}

// 大小端转换
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

// 向磁盘写一个block
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

// 从磁盘读一个block
void block_read(unsigned int block_num, void* buf)
{
    if(lseek(fsfd, BLOCK_SIZE * block_num, 0) != BLOCK_SIZE * block_num) {
        perror("lsek");
        exit(1);
    }
    if(read(fsfd, buf, BLOCK_SIZE) != BLOCK_SIZE) {
        perror("read");
        exit(1);
    }
}

// 申请一个block(修改bitmap)
unsigned int block_alloc()
{
    char buf[BLOCK_SIZE];
    unsigned int byte, shift;
    unsigned char bit_cmp;

    block_read(sb.data_bitmap_start, buf);
    for(byte = 0; byte < BLOCK_SIZE; byte++) {
        bit_cmp = 1;
        for(shift = 0; shift <= 7; shift++) {
            if((bit_cmp & buf[byte]) == 0) {
                buf[byte] |= bit_cmp;
                goto find;
            }
            bit_cmp = bit_cmp << 1;
        }
    }
    printf("block_alloc: no bit left\n");
    while(1);
find:
    block_write(sb.data_bitmap_start, buf);
    return byte * 8 + shift + sb.data_start;
}


// 从磁盘读一个inode
void inode_read(unsigned int inode_num, inode_disk_t* ip)
{
    char buf[BLOCK_SIZE];
    inode_disk_t* dip;

    unsigned int block_num = INODE_LOCATE_BLOCK(inode_num, sb);
    block_read(block_num, buf);
    dip = ((inode_disk_t*)buf) + (inode_num % INODE_PER_BLOCK);
    *ip = *dip;
}

// 向磁盘写一个inode
void inode_write(unsigned short inode_num, inode_disk_t* ip)
{
    char buf[BLOCK_SIZE];
    inode_disk_t* dip;

    unsigned int block_num = INODE_LOCATE_BLOCK(inode_num, sb);
    block_read(block_num, buf);
    dip = ((inode_disk_t*)buf) + (inode_num % INODE_PER_BLOCK);
    *dip = *ip;
    block_write(block_num, buf);
}

// 申请一个inode (修改bitmap)
unsigned short inode_alloc()
{
    char buf[BLOCK_SIZE];
    unsigned int byte, shift;
    unsigned char bit_cmp;

    block_read(sb.inode_bitmap_start, buf);
    for(byte = 0; byte < BLOCK_SIZE; byte++) {
        bit_cmp = 1;
        for(shift = 0; shift <= 7; shift++) {
            if((bit_cmp & buf[byte]) == 0) {
                buf[byte] |= bit_cmp;
                goto find;
            }
            bit_cmp = bit_cmp << 1;
        }
    }
    printf("inode_alloc: no bit left\n");
    while(1);
find:
    block_write(sb.inode_bitmap_start, buf);
    return (unsigned short)(byte * 8 + shift);
}

// 赋值并写一个inode
void inode_create(inode_disk_t* inode, unsigned int inode_num, unsigned short type)
{
    inode->type = xshort(type);
    inode->major = xshort(0);
    inode->minor = xshort(0);
    inode->nlink = xshort(1);
    inode->size = xint(0);
    for(int i = 0; i < 13; i++)
        inode->addrs[i] = 0;
    inode_write(inode_num, inode);
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

    // 准备根目录
    inode_disk_t rooti;
    unsigned short root_inum = inode_alloc();
    if(root_inum != 0) {
        printf("rooti = %d\n", root_inum);
        while(1);
    }
    inode_create(&rooti, root_inum, FT_DIR);

    // 准备 . 和 ..
    memset(buf, 0, BLOCK_SIZE);
    dirent_t* de;
    de = (dirent_t*)buf;
    de->inode_num = xshort(root_inum);
    strcpy(de->name, ".");
    de = (dirent_t*)(buf + sizeof(dirent_t));
    de->inode_num = xshort(root_inum);
    strcpy(de->name, "..");

    // 向根目录里写入
    unsigned int rooti_block = block_alloc();
    block_write(rooti_block, buf);

    // 更新rooti
    rooti.addrs[0] = xint(rooti_block);
    rooti.size = xint(sizeof(dirent_t) * 2);
    inode_write(root_inum, &rooti);

    return 0;
}