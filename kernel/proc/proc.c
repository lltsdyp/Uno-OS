#include "lib/print.h"
#include "lib/str.h"
#include "mem/pmem.h"
#include "mem/vmem.h"
#include "proc/cpu.h"
#include "proc/initcode.h"
#include "memlayout.h"

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
    vm_mappages(pgtbl, TRAMPOLINE, TRAMPOLINE_BASE_PA, TRAMPOLINE_SIZE, PTE_V | PTE_R);
    vm_mappages(pgtbl, TRAPFRAME, trapframe, PGSIZE, PTE_W | PTE_R);
}

// 初始化用户页表
// pgtbl:给定的用户页表
void user_pagetable_alloc(pgtbl_t pgtbl)
{
    uint64 addr = 0;
    addr = (uint64)pmem_alloc(false);
    // ustack 映射 + 设置 ustack_pages
    for (int i = 0; i < USER_STACK_INITIAL_PAGE_COUNT; ++i)
    {
        vm_mappages(pgtbl, PGROUNDDOWN(USER_STACK_BOTTOM - i * PGSIZE),
                    (uint64)pmem_alloc(false), PGSIZE, PTE_W | PTE_R | PTE_U);
    }

    // data + code 映射
    assert(initcode_len <= PGSIZE, "proc_make_first: initcode too big\n");

    vm_mappages(pgtbl, USER_VMEM_START, (uint64)pmem_alloc(false),
                PGSIZE, PTE_U | PTE_R | PTE_X | PTE_W);
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
void proc_make_fisrt()
{
    uint64 page;

    // pid 设置
    proczero.pid = 0;

    // pagetable 初始化
    page = (uint64)pmem_alloc(true);
    proczero.pgtbl = proc_pgtbl_init(page);

    // ustack 映射 + 设置 ustack_pages
    proczero.ustack_pages = USER_STACK_INITIAL_PAGE_COUNT;

    // data + code 映射
    assert(initcode_len <= PGSIZE, "proc_make_first: initcode too big\n");

    // 设置 heap_top
    proczero.heap_top = USER_VMEM_START + PGSIZE;

    // tf字段设置
    proczero.tf = (trapframe_t *)page;

    // 内核字段设置
    kstack_init();
    proczero.kstack = KSTACK(0);

    // 上下文切换
    swtch(&(mycpu()->ctx), &(myproc()->ctx));
}