# Uno-OS

## 简介
Uno-OS是一个基于MIT的xv6实验开发的，RISCV架构的简化版UNIX操作系统。

### 用户堆空间伸缩

在用户堆扩展时，首先计算扩展后的新的堆顶地址，然后通过一个 for 循环为新的堆空间申请物理页。

`vm_mappages` 函数的主要功能是建立虚拟地址 va 到物理地址 pa 的映射，映射范围为 `[va, va + len)`。该函数的五个参数从左到右分别是进程页表、起始虚拟地址、起始物理地址、映射的长度以及映射的权限位（如可读、可写等）。

在映射物理页面后，还需要对申请到的物理页进行初始化。

在用户堆收缩时，为避免传入的堆顶地址不是 PGSIZE 的整数倍，需要对两个地址进行对齐操作。

使用 `assert` 语句确保新的堆顶地址不低于预设的堆的起始地址`（USER_VMEM_START）`，以防止非法的内存访问。同时只需遍历从当前堆顶到新堆顶之间的所有物理页，并进行解映射操作以释放这些页面，从而实现堆空间的收缩。
```c
// 用户堆空间增加, 返回新的堆顶地址 (注意栈顶最大值限制)
// 在这里无需修正 p->heap_top
uint64 uvm_heap_grow(pgtbl_t pgtbl, uint64 heap_top, uint32 len)
{
    uint64 new_heap_top = heap_top + len;
    uint64 ptr;
    void *pg;

    for (ptr = heap_top; ptr < new_heap_top; ptr += PGSIZE)
    {
        pg = pmem_alloc(false);

        assert(pg != NULL, "uvm_heap_grow failed");

        vm_mappages(pgtbl, ptr, (uint64)pg, PGSIZE, PTE_W | PTE_R | PTE_U);
        memset(pg, 0, PGSIZE);
    }

    return new_heap_top;
}

// 用户堆空间减少, 返回新的堆顶地址
// 在这里无需修正 p->heap_top
uint64 uvm_heap_ungrow(pgtbl_t pgtbl, uint64 heap_top, uint32 len)
{
    uint64 new_heap_top = heap_top - len;

    // 将新的堆顶对齐到页面边界
    uint64 aligned_new_heap_top = PGROUNDUP(new_heap_top);
    uint64 ptr = PGROUNDUP(heap_top);

    // 在减少堆空间时，new_heap_top 可能会低于堆的最低起始地址，避免错误地释放不属于堆的页面
    assert(new_heap_top >= USER_VMEM_START, "uvm_heap_ungrow: new heap top out of range");

    // 遍历从当前堆顶到新的堆顶之间的所有页
    while (ptr > aligned_new_heap_top)
    {
        ptr -= PGSIZE;
        vm_unmappages(pgtbl, ptr, PGSIZE, true); // 解除映射并释放物理页
    }

    return new_heap_top;
}
```

### mmap_region_node 仓库管理
仓库管理也主要分为初始化、空间申请和空间归还三部分。        

在初始化时，由于仓库维护的是可分配的内存，所以链表的每个节点都指向那块空间的起始地址，然后等要分配空间时，再给那些节点申请相应的空间，并将节点移出仓库即可。
```c
// 包装 mmap_region_t 用于仓库组织
typedef struct mmap_region_node {
    mmap_region_t mmap;
    struct mmap_region_node* next;
} mmap_region_node_t;

// #define N_MMAP 256
#define N_MMAP 64

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
```
申请空间时，只需要将当前`list_head`维护的节点放出即可，因为现在分配的物理页页数都为0，所以具体实现操作就和普通的链表删除一样。

归还节点时，因为采用的是头插法策略，所以肯定要计算当前要归还的节点的位置，然后将仓库现有的可分配空间节点都链接到现在插入的节点后面，然后更新`list_head`即可。
```c
// 若申请失败则 panic
// 注意: list_head 保留, 不会被申请出去
mmap_region_t* mmap_region_alloc()
{
    spinlock_acquire(&list_lk);
    mmap_region_node_t* region = list_head->next;
    
    assert(region != NULL, "mmap_region_alloc failed");

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
```