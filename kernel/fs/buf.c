#include "fs/buf.h"
#include "dev/vio.h"
#include "lib/lock.h"
#include "lib/print.h"
#include "lib/str.h"

#define N_BLOCK_BUF 64
#define BLOCK_NUM_UNUSED 0xFFFFFFFF

// 将buf包装成双向循环链表的node
typedef struct buf_node {
    buf_t buf;
    struct buf_node* next;
    struct buf_node* prev;
} buf_node_t;

// buf cache
static buf_node_t buf_cache[N_BLOCK_BUF];
static buf_node_t head_buf; // ->next 已分配 ->prev 可分配
static spinlock_t lk_buf_cache; // 这个锁负责保护 链式结构 + buf_ref + block_num

// 链表操作
static void insert_head(buf_node_t* buf_node, bool head_next)
{
    // 离开
    if(buf_node->next && buf_node->prev) {
        buf_node->next->prev = buf_node->prev;
        buf_node->prev->next = buf_node->next;
    }

    // 插入
    if(head_next) { // 插入 head->next
        buf_node->prev = &head_buf;
        buf_node->next = head_buf.next;
        head_buf.next->prev = buf_node;
        head_buf.next = buf_node;        
    } else { // 插入 head->prev
        buf_node->next = &head_buf;
        buf_node->prev = head_buf.prev;
        head_buf.prev->next = buf_node;
        head_buf.prev = buf_node;
    }
}

// 初始化
void buf_init()
{
    spinlock_init(&lk_buf_cache, "buf_cache");
    
    head_buf.prev = &head_buf;
    head_buf.next = &head_buf;
    for(buf_node_t *buf_node=buf_cache; buf_node<buf_cache+N_BLOCK_BUF; buf_node++)
    {
        sleeplock_init(&buf_node->buf.slk, "buf_slk");
        insert_head(buf_node, 1);
    }
}

/*
    首先假设这个block_num对应的block在内存中有备份, 找到它并上锁返回
    如果找不到, 尝试申请一个无人使用的buf, 去磁盘读取对应block并上锁返回
    如果没有空闲buf, panic报错
    (建议合并xv6的bget())
*/
buf_t* buf_read(uint32 block_num)
{
    buf_t *target=NULL;
    spinlock_acquire(&lk_buf_cache);

    // 首先，我们寻找是否有已经缓存了block_num块对应的buf块
    for(buf_node_t *buf_node=head_buf.next;buf_node!=&head_buf;buf_node=buf_node->next)
    {
        if(buf_node->buf.block_num == block_num && buf_node->buf.disk==true)
        {
            
            buf_node->buf.buf_ref++;
            spinlock_release(&lk_buf_cache);
            sleeplock_acquire(&(buf_node->buf.slk));
            target=&(buf_node->buf);
        }
    }

    // 如果没找到，那么找一个空闲的buf块
    for(buf_node_t *buf_node=head_buf.prev;buf_node!=&head_buf;buf_node=buf_node->prev)
    {
        // 找到一个块
        if(buf_node->buf.buf_ref==0)
        {
            buf_node->buf.disk=1;
            buf_node->buf.block_num=block_num;
            buf_node->buf.buf_ref=1;
            spinlock_release(&lk_buf_cache);
            sleeplock_acquire(&(buf_node->buf.slk));
            virtio_disk_rw(target, 0);
            target=&(buf_node->buf);
        }
    }

    assert(target!=NULL, "buf_read: no buf available");
    return target;
}

// 写函数 (强制磁盘和内存保持一致)
void buf_write(buf_t* buf)
{
    assert(sleeplock_holding(&(buf->slk)),"buf_write: buf is not locked");

    virtio_disk_rw(buf, 1);
}

// buf 释放
void buf_release(buf_t* buf)
{
    assert(sleeplock_holding(&(buf->slk)),"buf_release: buf is not locked");
    sleeplock_release(&(buf->slk));

    spinlock_acquire(&lk_buf_cache);
    buf->buf_ref--;
    // 当前是最后一个使用这个buf块的
    if(buf->buf_ref==0)
    {
        // 寻找对应的位置
        buf_node_t *buf_node=head_buf;
        spinlock_acquire(&lk_buf_cache);
        while(&(buf_node->buf)!=buf)
            buf_node=buf_node->next;
        assert(buf_node!=&head_buf, "buf_release: buf not found");
        // 尾插
        insert_head(&(buf->node), 0);   
        spinlock_release(&lk_buf_cache);
    }
    spinlock_release(&lk_buf_cache);
}
