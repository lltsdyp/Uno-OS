#include "lib/print.h"
#include "lib/str.h"
#include "mem/pmem.h"
#include "mem/vmem.h"
#include "proc/cpu.h"
#include "proc/initcode.h"
#include "memlayout.h"
#include "mem/mmap.h"

// in trampoline.S
extern char trampoline[];

// in swtch.S
extern void swtch(context_t *old, context_t *new);

// in trap_user.c
extern void trap_user_return();

// 第一个进程
static proc_t proczero;

// 获得一个初始化过的用户页表
// 完成了trapframe 和 trampoline 的映射
pgtbl_t proc_pgtbl_init(uint64 trapframe)
{
    pgtbl_t pgtbl = pmem_alloc(true);
    memset(pgtbl, 0, PGSIZE);
    vm_mappages(pgtbl, TRAMPOLINE, (uint64)trampoline, TRAMPOLINE_SIZE, PTE_X | PTE_R);
    vm_mappages(pgtbl, TRAPFRAME, trapframe, PGSIZE, PTE_W | PTE_R);

    // ustack 映射 + 设置 ustack_pages
    for (int i = 0; i < USER_STACK_INITIAL_PAGE_COUNT; ++i)
    {
        vm_mappages(pgtbl, PGROUNDDOWN(USER_STACK_BOTTOM - (i+1) * PGSIZE),
                    (uint64)pmem_alloc(true), PGSIZE, PTE_W | PTE_R | PTE_U);
    }

    proczero.tf->sp=USER_STACK_BOTTOM;
    proczero.ctx.sp=KSTACK(proczero.pid)+PGSIZE;
    proczero.ustack_pages = USER_STACK_INITIAL_PAGE_COUNT;

    return pgtbl;
}

// 将initcode加载到内存中
// pgtbl:给定的用户页表
void load_initcode(pgtbl_t pgtbl)
{
    uint64 addr=0;

    // data + code 映射
    assert(initcode_len <= PGSIZE, "proc_make_first: initcode too big\n");

    vm_mappages(pgtbl, USER_VMEM_START, addr=(uint64)pmem_alloc(false),
                PGSIZE, PTE_U | PTE_R | PTE_X | PTE_W);
    memcpy((void *)addr,initcode,initcode_len);
}

/*
    第一个用户态进程的创建
    它的代码和数据位于initcode.h的initcode数组

    第一个进程的用户地址空间布局:
    trapoline   (1 page)
    trapframe   (1 page)
    ustack      (1 page)
    .......
                        <--heap_top
    code + data (1 page)
    empty space (1 page) 最低的4096字节 不分配物理页，同时不可访问
*/
void proc_make_first()  //TODO:增加mmap支持
{
    // pid 设置
    proczero.pid = 0;

    // pagetable 初始化
    proczero.tf = (trapframe_t *)pmem_alloc(false);
    proczero.pgtbl = proc_pgtbl_init((uint64)proczero.tf);

    // 申请mmap区域
    proczero.mmap=mmap_region_alloc();
    proczero.mmap->begin=MMAP_BEGIN;
    proczero.mmap->npages=0;//特殊标记节点，表示开始
    proczero.mmap->next=mmap_region_alloc();
    proczero.mmap->next->begin=MMAP_BEGIN;
    proczero.mmap->next->npages=(MMAP_END-MMAP_BEGIN)/PGSIZE;

    // 加载程序
    load_initcode(proczero.pgtbl);

    // 设置 heap_top
    proczero.heap_top = USER_VMEM_START + PGSIZE;

    // tf字段设置
    proczero.tf->epc = (uint64)USER_VMEM_START;
    proczero.ctx.ra=(uint64)trap_user_return;

    // 内核字段设置
    proczero.kstack = KSTACK(proczero.pid);

    // 上下文切换
    cpu_t *c = mycpu();
    c->proc = &proczero;
    swtch(&(mycpu()->ctx), &(proczero.ctx));
}
