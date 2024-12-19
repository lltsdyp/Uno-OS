#include "proc/cpu.h"
#include "mem/vmem.h"
#include "mem/pmem.h"
#include "mem/mmap.h"
#include "lib/str.h"
#include "lib/print.h"
#include "syscall/sysfunc.h"
#include "syscall/syscall.h"
#include "dev/timer.h"
#include "fs/bitmap.h"
#include "fs/buf.h"
#include "fs/fs.h"

// 打印字符
// uint64 addr
uint64 sys_print()
{
    char str[80*24];
    arg_str(0, str, 80*24);
    printf("%s",str);
    return 0;
}

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
        // printf("look: heap_tops = %p\n", p->heap_top);
        // vm_print(p->pgtbl);
        // printf("\n");
        return p->heap_top;
    }

    // 扩展堆
    else if (new_heap_top > p->heap_top)
    {
        size = new_heap_top - p->heap_top;
        p->heap_top = uvm_heap_grow(p->pgtbl, p->heap_top, size);

        // printf("grow: heap_tops = %p\n", p->heap_top);
        // vm_print(p->pgtbl);
        // printf("\n");
    }

    // 收缩堆
    else 
    {
        size = p->heap_top - new_heap_top;
        p->heap_top = uvm_heap_ungrow(p->pgtbl, p->heap_top, size);

        // printf("ungrow: heap_tops = %p\n", p->heap_top);
        // vm_print(p->pgtbl);
        // printf("\n");
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

// 进程复制
uint64 sys_fork()
{
    return proc_fork();
}

// 进程等待
// uint64 addr  子进程退出时的exit_state需要放到这里 
uint64 sys_wait()
{
    uint64 exit_addr;

    arg_uint64(0, &exit_addr);
    assert((void *)exit_addr!=NULL,"wait: exit state is null");
    return proc_wait(exit_addr);
}

// 进程退出
// int exit_state
uint64 sys_exit()
{
    uint32 exit_state;
    arg_uint32(0,&exit_state);

    proc_exit(exit_state);
    return (uint64)exit_state;
}

extern timer_t sys_timer;

// 进程睡眠一段时间
// uint32 second 睡眠时间
// 成功返回0, 失败返回-1
uint64 sys_sleep()
{
    uint32 sleeptime;
    arg_uint32(0, &sleeptime);

    spinlock_acquire(&(sys_timer.lk));
    uint64 start_tick=timer_get_ticks();
    while(timer_get_ticks() - start_tick < sleeptime)
    {
        proc_sleep((void *)&(sys_timer.ticks),&(sys_timer.lk));
    }
    spinlock_release(&(sys_timer.lk));
    return 0;
}

// 执行一个ELF文件
// char* path
// char** argv
// 成功返回argc 失败返回-1
uint64 sys_exec()
{
    char path[DIR_PATH_LEN];    // 文件路径
    char* argv[ELF_MAXARGS];    // 参数指针数组

}
