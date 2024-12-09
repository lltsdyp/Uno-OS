#ifndef __INODE_H__
#define __INODE_H__

#include "lib/lock.h"
#include "fs/dinode.h"

#define INODE_ROOT       0                              // 根节点的inode_num

// 每个block里面有多少个存储下一级block_num的entry
#define ENTRY_PER_BLOCK (BLOCK_SIZE / sizeof(uint32))

// addrs字段可以容纳的最大空间（单个inode可以管理的最大空间）
// 由于磁盘大小限制, 事实上达不到这个大小
#define MAX_FILE_SIZE ((N_ADDRS_1 + N_ADDRS_2 * ENTRY_PER_BLOCK + N_ADDRS_3 * ENTRY_PER_BLOCK * ENTRY_PER_BLOCK) * BLOCK_SIZE)

// inode_num 无效的inode号
#define INODE_NUM_UNUSED 0xFFFF

typedef struct inode {
    // 磁盘里的inode信息 (由slk保护)
    inode_disk_t disk_inode; //缓冲区，必要时读取或写入

    // 内存里的inode信息
    uint16 inode_num;           // inode序号
    uint32 ref;                 // 引用数 (由lk_icache保护)
    bool valid;                 // 上述磁盘里inode字段的有效性 (由slk保护)
    sleeplock_t slk;            // 睡眠锁

} inode_t;

// inode 元数据

void     inode_init();                        // 初始化
void     inode_rw(inode_t* ip, bool writeback);   // 读写inode元数据
inode_t* inode_get(uint16 inode_num);       // 在内存申请或查询inode(ref++)
inode_t* inode_create(uint16 type, uint16 major, uint16 minor); // 在磁盘里创建新的inode并在内存申请对应副本
void     inode_free(inode_t* ip);             // 释放inode(ref--) 适时销毁
inode_t* inode_dup(inode_t* ip);              // ref++
void     inode_lock(inode_t* ip);             // 上锁 (valid = false 则从磁盘读入inode)
void     inode_unlock(inode_t* ip);           // 解锁
void     inode_unlock_free(inode_t* ip);      // 解锁 + 释放

// inode 管理的数据

uint32   inode_read_data(inode_t* ip, uint32 offset, uint32 len, void* dst, bool user);
uint32   inode_write_data(inode_t* ip, uint32 offset, uint32 len, void* src, bool user);
void     inode_free_data(inode_t* ip);

// for debug

void     inode_print(inode_t* ip);

#endif