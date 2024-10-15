#include "mem/pmem.h"
#include "lib/print.h"
#include "lib/lock.h"
#include "lib/str.h"

// 物理页节点
typedef struct page_node {
    struct page_node* next;
} page_node_t;

// 许多物理页构成一个可分配的区域
typedef struct alloc_region {
    uint64 begin;          // 起始物理地址
    uint64 end;            // 终止物理地址
    spinlock_t lk;         // 自旋锁(保护下面两个变量)
    uint32 allocable;      // 可分配页面数    
    page_node_t list_head; // 可分配链的链头节点
} alloc_region_t;

// 内核和用户可分配的物理页分开
static alloc_region_t kern_region, user_region;

#define KERN_PAGES 1024 // 内核可分配空间占1024个pages

// 物理内存初始化
void pmem_init(void)
{

}

// 返回一个未使用的干净的物理页
// 失败则panic锁死
void* pmem_alloc(bool in_kernel)
{

}

// 释放物理页
// 失败则panic锁死
void pmem_free(uint64 page, bool in_kernel)
{    

}