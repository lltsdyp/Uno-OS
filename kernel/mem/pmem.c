#include "mem/pmem.h"
#include "lib/print.h"
#include "lib/lock.h"
#include "lib/str.h"

// 定义物理页节点，每个节点代表一个可用的物理页
typedef struct page_node
{
    struct page_node *next; // 指向下一个物理页节点
} page_node_t;

// 定义可分配的物理页区域，包括内核和用户区域
typedef struct alloc_region
{
    uint64 begin;          // 该区域的起始物理地址
    uint64 end;            // 该区域的终止物理地址
    spinlock_t lk;         // 自旋锁，用于保护以下两个变量
    uint32 allocable;      // 当前可分配的页面数量
    page_node_t list_head; // 可分配链表的头节点
} alloc_region_t;

// 定义内核和用户的可分配物理页区域
static alloc_region_t kern_region, user_region;

#define KERN_PAGES 1024 // 定义内核可分配空间的页数
// pgsize = 4096 Byte

// 物理内存初始化函数
void pmem_init(void)
{
    kern_region.begin = (uint64)ALLOC_BEGIN;
    kern_region.end = (uint64)ALLOC_BEGIN + (uint64)KERN_PAGES * PGSIZE;
    kern_region.allocable = (uint64)KERN_PAGES;
    spinlock_init(&kern_region.lk, "kern_region_lock");
    kern_region.list_head.next = NULL;
    freeRange(kern_region.begin, kern_region.end, true);

    user_region.begin = (uint64)ALLOC_BEGIN + (uint64)KERN_PAGES * PGSIZE;
    user_region.end = (uint64)ALLOC_END;
    user_region.allocable = ((uint64)ALLOC_END - (uint64)ALLOC_BEGIN) / PGSIZE - (uint64)KERN_PAGES;
    spinlock_init(&user_region.lk, "user_region_lock");
    user_region.list_head.next = NULL;
    freeRange(user_region.begin, user_region.end, false);

    // // 将内核/用户可用页面添加到链表中
    // for (uint64 addr = kern_region.begin; addr < kern_region.end; addr += PGSIZE)
    // {
    //     page_node_t *new_page = (page_node_t *)addr;
    //     new_page->next = kern_region.list_head.next;
    //     kern_region.list_head.next = new_page;
    // }

    // for (uint64 addr = user_region.begin; addr < user_region.end; addr += PGSIZE)
    // {
    //     page_node_t *new_page = (page_node_t *)addr;
    //     new_page->next = user_region.list_head.next;
    //     user_region.list_head.next = new_page;
    // }
}

// 从内核或用户区域返回一个未使用的干净的物理页
// 如果不足，触发panic并锁死程序
void *pmem_alloc(bool in_kernel)
{
    alloc_region_t *region = in_kernel ? &kern_region : &user_region;
    spinlock_acquire(&region->lk);

    // 检查当前区域的可分配页面数量是否为0，若为0则触发panic
    if (region->allocable == 0)
    {
        spinlock_release(&region->lk);
        panic("There is no empty page.");
        return NULL;
    }

    // 获取链表头部的下一个节点，即第一个可用的物理页
    page_node_t *page = region->list_head.next;
    region->list_head.next = page->next;
    --region->allocable;
    spinlock_release(&region->lk); 
    memset(page, 5, PGSIZE);
    return (void *)page;
}

// 释放物理页
// 如果给定的地址不合法则触发panic
void pmem_free(uint64 page, bool in_kernel)
{
    alloc_region_t *region = in_kernel ? &kern_region : &user_region;

    // 检查page是否在分配区域内
    if (page % PGSIZE != 0 || page < region->begin || page >= region->end)
    {
        panic("pmem_free: Invalid page address");
    }

    spinlock_acquire(&region->lk);

    // 通过使用memset将内存区域设置为垃圾值，可以确保在释放内存后，
    // 任何尝试访问该内存的操作都会失败，从而避免悬空引用的问题。
    memset((void *)page, 1, PGSIZE);

    page_node_t *free_page = (page_node_t *)page;
    free_page->next = region->list_head.next;
    region->list_head.next = free_page;
    ++region->allocable;

    spinlock_release(&region->lk);
}

void freeRange(uint64 begin, uint64 end, bool in_kernel)
{
    char *p;
    p = (char *)PGROUNDUP((uint64)begin);
    for (; p + PGSIZE <= (char *)end; p += PGSIZE)
        pmem_free((uint64)p, in_kernel);
}