#include "lib/print.h"
#include "lib/str.h"
#include "lib/lock.h"
#include "mem/pmem.h"
#include "mem/vmem.h"
#include "mem/mmap.h"
#include "memlayout.h"

// 包装 mmap_region_t 用于仓库组织
typedef struct mmap_region_node {
    mmap_region_t mmap;
    struct mmap_region_node* next;
} mmap_region_node_t;

#define N_MMAP 256

// mmap_region_node_t 仓库(单向链表) + 指向链表头节点的指针 + 保护仓库的锁
static mmap_region_node_t list_mmap_region_node[N_MMAP];
static mmap_region_node_t* list_head;
static spinlock_t list_lk;

// 初始化上述三个数据结构
void mmap_init()
{
    spinlock_init(&list_lk, "lk");
    list_head = list_mmap_region_node;
    for(int i = 0; i < N_MMAP; ++i)
    {
        list_mmap_region_node[i].mmap.begin = MMAP_BEGIN;
        list_mmap_region_node[i].mmap.npages = 0;

        // 还要判断是否是最后一个
        list_mmap_region_node[i].next = (i == (N_MMAP - 1)) ? NULL : &list_mmap_region_node[i + 1];
    }
}

// 从仓库申请一个 mmap_region_t
// 若申请失败则 panic
// 注意: list_head 保留, 不会被申请出去
mmap_region_t* mmap_region_alloc(bool init)
{
    spinlock_acquire(&list_lk);
    mmap_region_node_t* region = list_head->next;
    
    assert(region != NULL, "mmap_region_alloc failed");

    if(init)
    {
        region->mmap.begin = MMAP_BEGIN;
        region->mmap.npages = 0;
        region->mmap.next = NULL;
    }

    list_head->next = region->next;
    spinlock_release(&list_lk);

    return &(region->mmap);
}

// 向仓库归还一个 mmap_region_t
void mmap_region_free(mmap_region_t* mmap)
{
    spinlock_acquire(&list_lk);
    uint64 begin = (uint64)(&list_mmap_region_node->mmap);

    // 头插法
    int idx = ((uint64)mmap - begin) / sizeof(list_mmap_region_node[0]);
    list_mmap_region_node[idx].next = list_head->next;
    list_head->next = &list_mmap_region_node[idx];

    spinlock_release(&list_lk);
}

// 输出仓库里可用的 mmap_region_node_t
// for debug
void mmap_show_mmaplist()
{
    spinlock_acquire(&list_lk);
    
    mmap_region_node_t* tmp = list_head;
    int node = 1, index = 0;
    while (tmp)
    {
        index = tmp - list_head;
        printf("node %d index = %d\n", node++, index);
        tmp = tmp->next;
    }

    spinlock_release(&list_lk);
}