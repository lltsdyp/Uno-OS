# Uno-OS

## 简介
Uno-OS是一个基于MIT的xv6实验开发的，RISCV架构的简化版UNIX操作系统。

## 系统调用的过程
我们的系统实现系统调用的过程大致如下：
首先，`sys.h`中提供了用来为系统调用提供支持的宏，用宏的方式实现了一种类似“重载”定义了syscall函数的效果。
用户需要提供他想要调用的系统调用号以及传递的参数，这个系统调用号定义在`/include/syscall/sysnum.h`中。

`syscall`会将传递的参数以及系统调用号，按照RISCV中的约定，存储在`a0~a5`中（参数）以及`a7`中（系统调用号），并触发软中断，跳转到`user_vector`，随后的处理过程和典型的中断处理一致，我们在`trap_user_handler`中添加一个处理系统调用的分支，将系统调用交给`syscall()`函数处理

``` c
    // ...
    else {
        switch(trap_id)
        {
            case UMODE_SYSCALL_INTERRUPT:
                p->tf->epc += 4;
                intr_on();
                syscall();
                break;
            default:
                panic("trap_user_handler:Unknown trap id %x,\n\tdescription:%s", trap_id,exception_info[trap_id]);
                break;
        }
    }
```

syscall从trapframe中（由于进入了内核态，用户保存的参数和系统调用号都被存储在`trapframe`中）获取系统调用号和参数，然后通过系统调用表`syscalls`来获取具体需要调用的函数
``` c
static uint64 (*syscalls[])(void) = {
    [SYS_brk]           sys_brk,
    [SYS_mmap]          sys_mmap,
    [SYS_munmap]        sys_munmap,
    [SYS_copyin]        sys_copyin,
    [SYS_copyout]       sys_copyout,
    [SYS_copyinstr]     sys_copyinstr,
};
```

这些函数定义在`kernel/syscall/sysfunc.c`中。需要注意，这些函数的参数不是由syscall函数传递的，而是在用户态中传入寄存器，然后保存到了`trapframe`中，这些函数需要通过调用
``` c
void arg_uint32(int n, uint32* ip);
void arg_uint64(int n, uint64* ip);
void arg_str(int n, char* buf, int maxlen);
```
这些函数来实现参数的存取。

调用完成后，syscall会将返回值存储在trapframe的a0中，这样返回值在回到用户态后就会存储到`a0`中。

## 用户堆空间伸缩

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

## mmap_region_node 仓库管理
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

## mmap系列系统调用的实现
Uno-OS使用mmap来实现零散内存空间的分配，`mmap`和`munmap`两个系统调用实现了对可变内存区域`[MMAP_BEGIN ~ MMAP_END)`的管理（详见`/include/memlayout`）。管理的机制是一个链表，即`mmap_region_t`，每一个`mmap_region_t`实际上是链表的一个节点，他记录了一段连续的可分配可变内存区域。

proc_t中另外定义了一个`mmap`字段，他首先指向一个特殊的，管理可变页面数量为0的`mmap_region_t`（我们称为`dumb node`），这样有助于简化我们后面进行分配的操作。

当用户通过`mmap`系统调用请求一个内存区域时，他需要指定一个开始地址以及请求的页面数，当开始地址为0时，`mmap`会找出最靠前的适合的内存块分配给用户，否则使用指定的地址。`mmap`需要找到一个可以容纳整个请求内存块区域的`mmap_region_t`节点。由于我们保证连续内存区域被同一个`mmap_region_t`管理，如果我们找不到一个这样的区域，那么我们就报错。

具体分配时，有四种情况

- 开始地址和结束地址均不与当前的`mmap_region_t`开始地址和结束地址相同
- 开始地址相同但结束地址不同
- 结束地址相同但开始地址不同
- 两者均相同

情况2，3只需要调整`mmap_region_t`的边界就可以完成分配，情况1需要重新申请一个`mmap_region_t`来管理剩下的内存区域，因为原本连续的一个内存区域被切成了两个。情况4则需要删除当前`mmap_region_t`，因为整个连续的内存区域都被消耗掉。具体的操作方法见`/kernel/mem/uvm.c`

`munmap`会释放此前分配的连续内存区域，这个内存区域首先会形成一个新的`mmap_region_t`，然后，`munmap`会遍历链表，查找一个能够插入的位置，保证该链表按照每个节点管理的开始地址升序排列。然后，检查这个新的`mmap_region_t`是否与相邻节点的开始地址和结束地址相连，如果相连，那么我们合并这两个节点，这样我们就能保证连续内存区域被同一个`mmap_region_t`管理。

同样的，具体释放时也有四种情况

- 均不相邻
- 只有开头与前驱节点管理的内存区域相邻
- 只有结尾与后继节点管理的内存区域相邻
- 开头结尾均相邻

这四种情况的具体处理详见`/kernel/mem/uvm.c`，在此不作赘述。

如果节点应当被插入到链表尾端，那么我们注意只能检查其前驱节点，因为这种情况下检查后继节点将会引发空指针引用异常。

## 页表的复制和释放

`uvm_copy_pgtbl`将一个页表复制到另一个页表，同时将分配的物理页也一并拷贝，这个函数和用于销毁页表的`uvm_destroy_pgtbl`一样，都是在未来实现多进程时的辅助函数。

`uvm_copy_pgtbl`首先将从静态区域开始到堆顶的区域复制到新页表中，然后从最高位地址开始，复制到栈顶，对可变内存区域的分配则需要一些特殊的处理。根据上一节，我们知道，两个节点之间的区域是我们通过`mmap`分配掉的内存区域。由此得出**下一个节点的开始地址-上一个节点的结束地址**是分配掉的地址大小，上一个节点的结束地址是分配掉的内存块的起始地址，这样我们就知道了需要被释放的内存区域，将其释放掉即可
