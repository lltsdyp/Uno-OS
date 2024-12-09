#ifndef __DINODE_H__
#define __DINODE_H__

// addrs相关字段
#define N_ADDRS_1   10  // 管理 10 * BLOCK_SIZE = 10KB
#define N_ADDRS_2   2   // 管理 2 * (BLOCK_SIZE / 4) * BLOCK_SIZE = 512KB
#define N_ADDRS_3   1   // 管理 (BLOCK_SIZE / 4) * (BLOCK_SIZE / 4) * BLOCK_SIZE = 64MB
#define N_ADDRS     (N_ADDRS_1 + N_ADDRS_2 + N_ADDRS_3)

// inode 64 byte
typedef struct inode_disk {
    short type; // inode 管理的文件类型
    short major; // 设备文件使用: 主设备号
    short minor; // 设备文件使用: 次设备号
    short nlink; // 链接数量 (nlink个文件名链接到这个inode)
    unsigned int size; // 文件大小 (字节)
    unsigned int addrs[N_ADDRS]; // 文件存储在哪些block里 (分为一级 二级 三级)
} inode_disk_t;

// 文件类型
#define FT_UNUSED 0
#define FT_DIR    1
#define FT_FILE   2
#define FT_DEVICE 3 

// 常量定义 
#define INODE_DISK_SIZE  sizeof(inode_disk_t) // 磁盘中inode的大小
#define INODE_PER_BLOCK  (BLOCK_SIZE / sizeof(inode_disk_t)) // 每个block里的inode数量
// #define N_INODE          (N_INODE_BLOCK * INODE_PER_BLOCK)   // inode总数

// 确定inode所在的inode block序号
#define INODE_LOCATE_BLOCK(inum, sb)  ((inum) / INODE_PER_BLOCK + sb.inode_start)


#endif