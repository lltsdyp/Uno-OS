#include "fs/buf.h"
#include "fs/bitmap.h"
#include "fs/inode.h"
#include "fs/fs.h"
#include "mem/vmem.h"
#include "proc/cpu.h"
#include "lib/print.h"
#include "lib/str.h"

#include "fs/dinode.h"

extern super_block_t sb;

// 内存中的inode资源 + 保护它的锁
#define I_NODE_CACHE_SIZE 32
static inode_t icache[I_NODE_CACHE_SIZE];
static spinlock_t lk_icache;

// icache初始化
void inode_init()
{
    spinlock_init(&lk_icache, "icache lock");
    for (int i = 0; i < I_NODE_CACHE_SIZE; i++)
    {
        icache[i].inode_num = INODE_NUM_UNUSED;
        sleeplock_init(&(icache[i].slk), "inode lock");
    }
}

/*---------------------- 与inode本身相关 -------------------*/

// 使用磁盘里的inode更新内存里的inode (writeback = false)
// 或 使用内存里的inode更新磁盘里的inode (writeback = true)
// 调用者需要设置inode_num并持有睡眠锁
void inode_rw(inode_t *ip, bool writeback)
{
    if (writeback)
    {
        buf_t *b = buf_read(INODE_LOCATE_BLOCK(ip->inode_num, sb));
        memcpy((void *)((inode_disk_t *)(b->data) + ip->inode_num % INODE_PER_BLOCK),
               (void *)&(ip->disk_inode), INODE_DISK_SIZE);
        buf_write(b);
        buf_release(b);
    }
    else // read
    {
        buf_t *b = buf_read(INODE_LOCATE_BLOCK(ip->inode_num, sb));
        memcpy((void *)&(ip->disk_inode),
               (void *)((inode_disk_t *)(b->data) + ip->inode_num % INODE_PER_BLOCK),
               INODE_DISK_SIZE);
        buf_release(b);
    }
}

// 在icache里查询inode
// 如果没有查询到则申请一个空闲inode
// 如果icache没有空闲inode则报错
// 注意: 获得的inode没有上锁
inode_t *inode_get(uint16 inode_num)
{
    inode_t *empty_inode = NULL;

    spinlock_acquire(&lk_icache);
    for (int i = 0; i < I_NODE_CACHE_SIZE; ++i)
    {
        // 找到的情况
        if (icache[i].inode_num == inode_num && icache[i].inode_num > 0)
        {
            ++(icache[i].ref);
            spinlock_release(&lk_icache);
            return &icache[i];
        }

        // 缓存一下空的缓存位置备用
        if (empty_inode == NULL && icache[i].inode_num == INODE_NUM_UNUSED)
        {
            empty_inode = &icache[i];
        }
    }

    assert(empty_inode != NULL, "inode_get: no empty inode");

    empty_inode->inode_num = inode_num;
    empty_inode->ref = 1;
    empty_inode->valid = false;
    spinlock_release(&lk_icache);

    return empty_inode;
}

// 在磁盘里申请一个inode (操作bitmap, 返回inode_num)
// 向icache申请一个inode数据结构
// 填写内存里的inode并以此更新磁盘里的inode
// 注意: 获得的inode没有上锁
inode_t *inode_create(uint16 type, uint16 major, uint16 minor)
{
    assert(type == FT_DIR || type == FT_FILE || type == FT_DEVICE, "inode_create: invalid or reserved type id:%d", type);

    buf_t *b = NULL;
    inode_disk_t *disk_inode = NULL;

    uint16 inode_num = bitmap_alloc_inode();
    b = buf_read(INODE_LOCATE_BLOCK(inode_num, sb));
    disk_inode = (inode_disk_t *)(b->data) + inode_num % INODE_PER_BLOCK;

    memset(disk_inode, 0, INODE_DISK_SIZE);
    disk_inode->type = type;
    disk_inode->major = major;
    disk_inode->minor = minor;

    buf_write(b);
    buf_release(b);
    return inode_get(inode_num);
    panic("inode_create: no empty inode");
}

// 供inode_free调用
// 在磁盘上删除一个inode及其管理的文件 (修改inode bitmap + block bitmap)
// 调用者需要持有lk_icache, 但不应该持有slk **？**
static void inode_destroy(inode_t *ip)
{
    // 释放数据块
    inode_free_data(ip);
    // 释放inode
    bitmap_free_inode(ip->inode_num);
}

// 向icache里归还inode
// inode->ref--
// 调用者不应该持有slk **？**
void inode_free(inode_t *ip)
{
    assert(!sleeplock_holding(&(ip->slk)), "inode_free: unexpectedly hold sleeplock");
    spinlock_acquire(&lk_icache);

    if (ip->ref == 1 && ip->valid && ip->disk_inode.nlink == 0)
    {
        // 释放掉当前文件

        sleeplock_acquire(&(ip->slk));
        spinlock_release(&lk_icache);

        inode_destroy(ip);
        ip->disk_inode.type = FT_UNUSED;
        inode_rw(ip, true);
        ip->valid = 0;

        sleeplock_release(&(ip->slk));
        spinlock_acquire(&lk_icache);
    }

    --(ip->ref);
    spinlock_release(&lk_icache);
}

// ip->ref++ with lock
inode_t *inode_dup(inode_t *ip)
{
    spinlock_acquire(&lk_icache);
    ++(ip->ref);
    spinlock_release(&lk_icache);
    return ip;
}

// 给inode上锁
// 如果valid失效则从磁盘中读入
void inode_lock(inode_t *ip)
{
    assert(ip && ip->ref >= 1, "ilock: invalid inode or ref=0");

    sleeplock_acquire(&(ip->slk));

    if (!ip->valid)
    {
        inode_rw(ip, false);
        ip->valid = true;
        assert(ip->disk_inode.type == FT_DIR || ip->disk_inode.type == FT_FILE || ip->disk_inode.type == FT_DEVICE, "inode_lock: invalid or reserved type id:%d", ip->disk_inode.type);
    }
}

// 给inode解锁
void inode_unlock(inode_t *ip)
{
    assert(ip && sleeplock_holding(&(ip->slk)) && ip->ref >= 1, "inode_unlock: invalid inode or not holding lock");

    sleeplock_release(&(ip->slk));
}

// 解锁 + 释放
void inode_unlock_free(inode_t *ip)
{
    inode_unlock(ip);
    inode_free(ip);
}

/*---------------------------- 与inode管理的data相关 --------------------------*/

// 确定inode里第bn块data block的block_num
// 如果不存在第bn块data block则申请一个并返回它的block_num
// 由于inode->addrs的结构, 这个过程比较复杂, 需要单独处理
static uint32 inode_locate_block(inode_t *ip, uint32 bn)
{
    // 如果确保该函数只会被inode_write_data调用，那么下面这行可以删去
    assert(sleeplock_holding(&(ip->slk)), "inode_locate_block: not holding slk");

    uint32 result = 0;
    // 直接
    if (bn < N_ADDRS_1)
    {
        result = ip->disk_inode.addrs[bn];
        if (result == 0)
        {
            result = bitmap_alloc_block();
            assert(result != -1, "inode_locate_block: bitmap_alloc_block failed");
            ip->disk_inode.addrs[bn] = result;
        }
    }
    // 一级间接
    else if (bn < N_ADDRS_1 + N_ADDRS_2 * ENTRY_PER_BLOCK)
    {
        // 计算出位于哪个一级间接块
        int index1 = (bn - N_ADDRS_1) / ENTRY_PER_BLOCK + N_ADDRS_1;

        // 第一次寻址
        uint32 sub_block1 = ip->disk_inode.addrs[index1];
        if (sub_block1 == 0)
        {
            sub_block1 = bitmap_alloc_block();
            assert(sub_block1 != 0, "inode_locate_block: bitmap_alloc_block failed");
            ip->disk_inode.addrs[index1] = sub_block1;
        }

        // 第二次寻址
        int index2 = (bn - N_ADDRS_1) % ENTRY_PER_BLOCK;
        buf_t *sub_table1 = buf_read(sub_block1);
        result = *((uint32 *)sub_table1->data + index2);
        if (result == 0)
        {
            result = bitmap_alloc_block();
            assert(result != 0, "inode_locate_block: bitmap_alloc_block failed");
            *((uint32 *)sub_table1->data + index2) = result;
        }
        buf_write(sub_table1);
        buf_release(sub_table1);
    }
    // 二级间接
    else if (bn < N_ADDRS_1 + N_ADDRS_2 * ENTRY_PER_BLOCK + N_ADDRS_3 * ENTRY_PER_BLOCK * ENTRY_PER_BLOCK)
    {
        // 计算出位于哪个三级间接块
        int index1 = (bn - N_ADDRS_1 - ENTRY_PER_BLOCK * N_ADDRS_2) /
                         ENTRY_PER_BLOCK * ENTRY_PER_BLOCK +
                     N_ADDRS_1 + N_ADDRS_2;

        // 第一次寻址
        uint32 sub_block1 = ip->disk_inode.addrs[index1];
        if (sub_block1 == 0)
        {
            sub_block1 = bitmap_alloc_block();
            assert(sub_block1 != 0, "inode_locate_block: bitmap_alloc_block failed");
            ip->disk_inode.addrs[index1] = sub_block1;
        }

        // 第二次寻址
        int index2 = (bn - N_ADDRS_1 - ENTRY_PER_BLOCK * N_ADDRS_2) /
                     ENTRY_PER_BLOCK;
        uint32 sub_block2 = 0;
        buf_t *sub_table1 = buf_read(sub_block1);
        sub_block2 = *((uint32 *)sub_table1->data + index2);
        if (sub_block2 == 0)
        {
            sub_block2 = bitmap_alloc_block();
            assert(sub_block2 != 0, "inode_locate_block: alloc block failed");
            *((uint32 *)sub_table1->data + index2) = sub_block2;
        }
        buf_write(sub_table1);
        buf_release(sub_table1);

        int index3 = (bn - N_ADDRS_1 - ENTRY_PER_BLOCK * N_ADDRS_2) % ENTRY_PER_BLOCK;
        buf_t *sub_table2 = buf_read(sub_block2);
        result = *((uint32 *)sub_table2->data + index3);
        if (result == 0)
        {
            result = bitmap_alloc_block();
            assert(result != 0, "inode_locate_block: bitmap_alloc_block failed");
            *((uint32 *)sub_table2->data + index3) = result;
        }
        buf_write(sub_table2);
        buf_release(sub_table2);
    }
    else
    {
        panic("inode_locate_block: invalid block number");
    }

    // 更新size，只有这个函数会为inode分配新的block，因此更新逻辑放在此处
    // if(ip->disk_inode.size<(bn+1)*BLOCK_SIZE)
    // {
    //     ip->disk_inode.size=(bn+1)*BLOCK_SIZE;
    //     inode_rw(ip,true);
    // }
    return result;
}

// 读取 inode 管理的 data block
// 调用者需要持有 inode 锁
// 成功返回读出的字节数, 失败返回0
uint32 inode_read_data(inode_t *ip, uint32 offset, uint32 len, void *dst, bool user)
{
    assert(sleeplock_holding(&(ip->slk)), "inode_read_data: not holding slk");

    uint32 count = 0;
    uint32 total = len;

    inode_rw(ip, false);

    if (offset > ip->disk_inode.size || offset + len < offset)
        return -1;
    if (len + offset > ip->disk_inode.size)
        total = ip->disk_inode.size - offset;

    uint32 beg = offset;
    char *dst_by_byte = (char *)dst;

    while (count < total)
    {
        buf_t *b = buf_read(inode_locate_block(ip, beg / BLOCK_SIZE));

        // 确认本次读写的大小
        uint32 readsize = BLOCK_SIZE - beg % BLOCK_SIZE;
        if (readsize > total - count)
            readsize = total - count;

        if (user)
            uvm_copyout(myproc()->pgtbl, (uint64)dst_by_byte,
                        (uint64)b->data + beg % BLOCK_SIZE, readsize);
        else
            memcpy((void *)dst_by_byte, (void *)(b->data + beg % BLOCK_SIZE), readsize);

        buf_release(b);

        count += readsize;
        beg += readsize;
        dst_by_byte += readsize;
    }
    return count;
}

// 写入 inode 管理的 data block (可能导致管理的 block 增加)
// 调用者需要持有 inode 锁
// 成功返回写入的字节数, 失败返回0
uint32 inode_write_data(inode_t *ip, uint32 offset, uint32 len, void *src, bool user)
{
    assert(sleeplock_holding(&(ip->slk)), "inode_write_data: not holding slk");

    uint32 count = 0;

    if (offset + len < offset)
        return -1;
    if (offset + len > MAX_FILE_SIZE)
        return -1;

    uint32 beg = offset;
    char *src_by_byte = (char *)src;

    int size_modified = 0;

    while (count < len)
    {
        buf_t *b = buf_read(inode_locate_block(ip, beg / BLOCK_SIZE));
        uint32 writesize = BLOCK_SIZE - beg % BLOCK_SIZE;
        if (writesize > len - count)
        {
            writesize = len - count;
        }

        if (user)
            uvm_copyin(myproc()->pgtbl, (uint64)b->data + beg % BLOCK_SIZE, (uint64)src_by_byte, writesize);
        else
            memcpy((void *)(b->data + beg % BLOCK_SIZE), (void *)src_by_byte, writesize);

        buf_write(b);
        buf_release(b);

        beg += writesize;
        count += writesize;
        src_by_byte += writesize;

        if (offset + count > ip->disk_inode.size)
        {
            ip->disk_inode.size = offset + len;
            size_modified = 1;
        }
    }

    if (size_modified)
    {
        inode_rw(ip, true);
    }
    return count;
}

// 辅助 inode_free_data 做递归释放
static void data_free(uint32 block_num, uint32 level)
{
    assert(block_num != 0, "data_free: block_num = 0");

    // block_num 是 data block
    if (level == 0)
        goto ret;

    // block_num 是 metadata block
    buf_t *buf = buf_read(block_num);
    for (uint32 *addr = (uint32 *)buf->data; addr < (uint32 *)(buf->data + BLOCK_SIZE); addr++)
    {
        if (*addr == 0)
            break;
        data_free(*addr, level - 1);
    }
    buf_release(buf);

ret:
    bitmap_free_block(block_num);
    return;
}

// 释放inode管理的 data block
// ip->addrs被清空 ip->disk_inode.size置0
// 调用者需要持有slk
void inode_free_data(inode_t *ip)
{
    assert(sleeplock_holding(&(ip->slk)), "inode_free_data: not holding slk");

    // 直接寻址部分
    for (int i = 0; i < N_ADDRS_1; ++i)
    {
        if (ip->disk_inode.addrs[i])
        {
            data_free(ip->disk_inode.addrs[i], 0);
            ip->disk_inode.addrs[i] = 0;
        }
    }

    // 一级间接寻址
    for (int i = N_ADDRS_1; i < N_ADDRS_1 + N_ADDRS_2; ++i)
    {
        if (ip->disk_inode.addrs[i])
        {
            data_free(ip->disk_inode.addrs[i], 1);
            ip->disk_inode.addrs[i] = 0;
        }
    }

    // 二级间接寻址
    for (int i = N_ADDRS_1 + N_ADDRS_2; i < N_ADDRS; ++i)
    {
        if (ip->disk_inode.addrs[i])
        {
            data_free(ip->disk_inode.addrs[i], 2);
            ip->disk_inode.addrs[i] = 0;
        }
    }
}

static char *inode_types[] = {
    "INODE_UNUSED",
    "INODE_DIR",
    "INODE_FILE",
    "INODE_DEVICE",
};

// 输出inode信息
// for dubug
void inode_print(inode_t *ip)
{
    assert(sleeplock_holding(&ip->slk), "inode_print: lk");

    printf("\ninode information:\n");
    printf("num = %d, ref = %d, valid = %d\n", ip->inode_num, ip->ref, ip->valid);
    printf("type = %s, major = %d, minor = %d, nlink = %d\n", inode_types[ip->disk_inode.type], ip->disk_inode.major, ip->disk_inode.minor, ip->disk_inode.nlink);
    printf("size = %d, addrs =", ip->disk_inode.size);
    for (int i = 0; i < N_ADDRS; i++)
        printf(" %d", ip->disk_inode.addrs[i]);
    printf("\n\n");
}