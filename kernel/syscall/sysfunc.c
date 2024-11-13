#include "proc/cpu.h"
#include "mem/vmem.h"
#include "mem/pmem.h"
#include "mem/mmap.h"
#include "lib/str.h"
#include "lib/print.h"
#include "syscall/sysfunc.h"
#include "syscall/syscall.h"

// 堆伸缩
// uint64 new_heap_top 新的堆顶 (如果是0代表查询, 返回旧的堆顶)
// 成功返回新的堆顶 失败返回-1
uint64 sys_brk()
{
    proc_t *p = myproc();
    uint64 new_heap_top;
    uint64 size;
    
    // 读取新堆顶
    arg_uint64(0, &new_heap_top);

    // 查询堆顶
    if(new_heap_top == 0)
    {
        printf("look: heap_tops = %p\n", p->heap_top);
        vm_print(p->pgtbl);
        printf("\n");
        return p->heap_top;
    }

    // 扩展堆
    else if (new_heap_top > p->heap_top)
    {
        size = new_heap_top - p->heap_top;
        p->heap_top = uvm_heap_grow(p->pgtbl, p->heap_top, size);

        printf("grow: heap_tops = %p\n", p->heap_top);
        vm_print(p->pgtbl);
        printf("\n");
    }

    // 收缩堆
    else 
    {
        size = p->heap_top - new_heap_top;
        p->heap_top = uvm_heap_ungrow(p->pgtbl, p->heap_top, size);

        printf("ungrow: heap_tops = %p\n", p->heap_top);
        vm_print(p->pgtbl);
        printf("\n");
    }

    return p->heap_top;
}


// 内存映射
// uint64 start 起始地址 (如果为0则由内核自主选择一个合适的起点, 通常是顺序扫描找到一个够大的空闲空间)
// uint32 len   范围(字节, 检查是否是page-aligned)
// 成功返回映射空间的起始地址, 失败返回-1
uint64 sys_mmap()
{
    uint64 begin;
    uint32 npages;
    arg_uint64(0, &begin);
    arg_uint32(1, &npages);
    uvm_mmap(begin, npages/PGSIZE, PTE_U|PTE_R|PTE_W);
    return begin;
}

// 取消内存映射
// uint64 start 起始地址
// uint32 len   范围(字节, 检查是否是page-aligned)
// 成功返回0 失败返回-1
uint64 sys_munmap()
{
    uint64 begin;
    uint32 npages;
    arg_uint64(0, &begin);
    arg_uint32(1, &npages);
    uvm_munmap(begin, npages/PGSIZE);
    return 0;
}

// copyin 测试 (int 数组)
// uint64 addr
// uint32 len
// 返回 0
uint64 sys_copyin()
{
    proc_t* p = myproc();
    uint64 addr;
    uint32 len;

    arg_uint64(0, &addr);
    arg_uint32(1, &len);

    int tmp;
    for(int i = 0; i < len; i++) {
        uvm_copyin(p->pgtbl, (uint64)&tmp, addr + i * sizeof(int), sizeof(int));
        printf("get a number from user: %d\n", tmp);
    }

    return 0;
}

// copyout 测试 (int 数组)
// uint64 addr
// 返回数组元素数量
uint64 sys_copyout()
{
    int L[5] = {1, 2, 3, 4, 5};
    proc_t* p = myproc();
    uint64 addr;

    arg_uint64(0, &addr);
    uvm_copyout(p->pgtbl, addr, (uint64)L, sizeof(int) * 5);

    return 5;
}

// copyinstr测试
// uint64 addr
// 成功返回0
uint64 sys_copyinstr()
{
    char s[64];

    arg_str(0, s, 64);
    printf("get str from user: %s\n", s);

    return 0;
}
