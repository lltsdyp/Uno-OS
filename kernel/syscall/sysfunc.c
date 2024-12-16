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

// // copyin 测试 (int 数组)
// // uint64 addr
// // uint32 len
// // 返回 0
// uint64 sys_copyin()
// {
//     proc_t* p = myproc();
//     uint64 addr;
//     uint32 len;

//     arg_uint64(0, &addr);
//     arg_uint32(1, &len);

//     int tmp;
//     for(int i = 0; i < len; i++) {
//         uvm_copyin(p->pgtbl, (uint64)&tmp, addr + i * sizeof(int), sizeof(int));
//         printf("get a number from user: %d\n", tmp);
//     }

//     return 0;
// }

// // copyout 测试 (int 数组)
// // uint64 addr
// // 返回数组元素数量
// uint64 sys_copyout()
// {
//     int L[5] = {1, 2, 3, 4, 5};
//     proc_t* p = myproc();
//     uint64 addr;

//     arg_uint64(0, &addr);
//     uvm_copyout(p->pgtbl, addr, (uint64)L, sizeof(int) * 5);

//     return 5;
// }

// // copyinstr测试
// // uint64 addr
// // 成功返回0
// uint64 sys_copyinstr()
// {
//     char s[64];

//     arg_str(0, s, 64);
//     printf("get str from user: %s\n", s);

//     return 0;
// }


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


// temporarily
extern super_block_t sb;

// 申请一个block
// 返回申请到的block的序号
uint64 sys_alloc_block()
{
    uint32 ret = bitmap_alloc_block();
    bitmap_print(sb.data_bitmap_start); 
    return ret;
}

// 释放一个block
// uint32 block_num 要释放的block序号
// 成功返回0
uint64 sys_free_block()
{
    uint32 block_num;
    arg_uint32(0, &block_num);
    bitmap_free_block(block_num);
    bitmap_print(sb.data_bitmap_start);
    return 0;
}

// 测试 buf_read
// uint32 block_num 要被读取的block序号
// uint64 addr 内容放入用户的这个地址
// 成功返回buf的地址
uint64 sys_read_block()
{
    uint32 block_num;
    uint64 addr;
    arg_uint32(0, &block_num);
    arg_uint64(1, &addr);

    buf_t* buf = buf_read(block_num);
    uvm_copyout(myproc()->pgtbl, addr, (uint64)(buf->data), 128);
    return (uint64)buf;
}

// 修改block
// uint64 buf_addr buf的地址
// uint64 write_addr 用户希望写入的数据地址
// 返回0
uint64 sys_write_block()
{
    uint64 buf_addr, write_addr;
    arg_uint64(0, &buf_addr);
    arg_uint64(1, &write_addr);

    buf_t* buf = (buf_t*)(buf_addr);
    uvm_copyin(myproc()->pgtbl, (uint64)(buf->data), write_addr, 128);

    return 0;
}

// 测试 buf_release
// uint64 buf_addr buf的地址
uint64 sys_release_block()
{
    uint64 buf_addr;
    arg_uint64(0, &buf_addr);

    buf_t* buf = (buf_t*)(buf_addr);
    buf_release(buf);
    
    return 0;
}

// 测试 buf_print
uint64 sys_show_buf()
{
    buf_print();
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
