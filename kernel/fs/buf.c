#include "fs/buf.h"
#include "dev/vio.h"
#include "lib/lock.h"
#include "lib/print.h"
#include "lib/str.h"

#define N_BLOCK_BUF 512
#define BLOCK_NUM_UNUSED 0xFFFFFFFF

// buf cache
static buf_t buf_cache[N_BLOCK_BUF];
static buf_t head_buf; // ->next 已分配 ->prev 可分配
static spinlock_t lk_buf_cache; // 这个锁负责保护 链式结构 + buf_ref + block_num

// 调试用，检查是否有buf 未释放
void get_free_buf()
{
    int count=1;
    for(buf_t *node=head_buf.prev;node->buf_ref==0&&node!=&head_buf;node=node->prev)
    {
        ++count;
    }
    printf("free buf:%d",count);
}

// 链表操作
static void insert_head(buf_t* buf_node, bool head_next)
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
    for(buf_t *buf_node=buf_cache; buf_node<buf_cache+N_BLOCK_BUF; buf_node++)
    {
        sleeplock_init(&buf_node->slk, "buf_slk");
        buf_node->block_num = BLOCK_NUM_UNUSED;
        insert_head(buf_node, 1);
    }
}

/*
    首先假设这个block_num对应的block在内存中有备份, 找到它并上锁返回
    如果找不到, 尝试申请一个无人使用的buf, 去磁盘读取对应block并上锁返回
    如果没有空闲buf, panic报错
*/
buf_t* buf_read(uint32 block_num)
{
    // printf("buf_read: buf %d allocated\n",block_num);
    buf_t *target=NULL,*buf_node=head_buf.prev,*oldest_node=NULL;
    spinlock_acquire(&lk_buf_cache);

    // 局部性原理
    while(target==NULL&&buf_node->buf_ref==0&&buf_node!=&head_buf)
    {
        // 找到一个块
        if(buf_node->buf_ref==0)
        {
            insert_head(buf_node,1);
            target=buf_node;
            target->block_num=block_num;
            target->buf_ref=1;
            spinlock_release(&lk_buf_cache);
            sleeplock_acquire(&(target->slk));
            virtio_disk_rw(target, 0);
        }
    }
    // 避免多次遍历链表
    oldest_node=buf_node->next;

    buf_node=head_buf.next;
    while(target==NULL&&buf_node!=oldest_node)
    {
        if(buf_node->block_num == block_num)
        {
            insert_head(buf_node, 1);
            target=buf_node;
            target->buf_ref++;
            spinlock_release(&lk_buf_cache);
            sleeplock_acquire(&(target->slk));
        }
        buf_node=buf_node->next;
    }

    if(target==NULL && oldest_node!=&head_buf)
    {
        target=oldest_node;
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
    // printf("buf_release: buf %d released\n", buf->block_num);
    assert(sleeplock_holding(&(buf->slk)),"buf_release: buf is not locked");
    sleeplock_release(&(buf->slk));

    spinlock_acquire(&lk_buf_cache);
    buf->buf_ref--;
    // 当前是最后一个使用这个buf块的
    if(buf->buf_ref==0)
    {
        // 尾插
        insert_head(buf, 0);   
    }
    spinlock_release(&lk_buf_cache);
}

void buf_print()
{
    printf("\nbuf_cache:\n");
    buf_t *b;
    for (b = head_buf.next; b != &head_buf; b = b->next)
    {
        printf("buf %d: ref = %d, block_num = %d\n", b - buf_cache, b->buf_ref, b->block_num);
        for (int i = 0; i < 8; i++)
            printf("%d ", b->data[i]);
        printf("\n");
    }
}
