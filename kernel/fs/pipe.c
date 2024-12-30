#include "fs/pipe.h"
#include "fs/file.h"
#include "mem/pmem.h"
#include "mem/vmem.h"
#include "lib/str.h"
#include "lib/lock.h"
#include "proc/proc.h"
#include "proc/cpu.h"
#include "lib/print.h"

// 新建一个管道
// 成功返回
int pipe_alloc(file_t **f1,file_t **f2)
{
    assert(f1&&f2,"pipe_alloc: Invalid file pointer");
    pipe_t *pipe = pmem_alloc(1);

    // pipe_t初始化
    spinlock_init(&pipe->lock, "pipe");
    pipe->readopen=1;
    pipe->writeopen=1;
    pipe->nread=0;
    pipe->nwrite=0;

    // 相应的文件初始化
    *f1=file_alloc();
    *f2=file_alloc();
    if(f1 && f2)
    {
        (*f1)->type=FD_PIPE;
        (*f1)->writable=0;
        (*f1)->readable=1;
        (*f1)->pipe=pipe;

        (*f2)->type=FD_PIPE;
        (*f2)->writable=1;
        (*f2)->readable=0;
        (*f2)->pipe=pipe;
        return 0;
    }

    // 失败后会执行此处代码
    if(*f1)
        file_close(*f1);
    if(*f2)
        file_close(*f2);
    return -1;
}

// 写入一个管道
uint32 pipe_write(pipe_t *pipe, uint64 addr, uint32 len)
{
    assert(pipe!=NULL,"pipe_write: Invalid pipe pointer");
    int i=0;
    char ch;
    if(len==0)
        return 0;

    spinlock_acquire(&pipe->lock);
    for(i=0;i<len;i++)
    {
        // 处理管道已满的情况
        while (pipe->nwrite-pipe->nread==PIPE_SIZE)
        {
            if(!pipe->readopen)
            {
                spinlock_release(&pipe->lock);
                return PIPE_ERROR;
            }
            // 实际上就是一个很经典的生产者/消费者问题
            proc_wakeup(&pipe->nread);
            proc_sleep(&pipe->nwrite,&pipe->lock);
        }
        uvm_copyin(myproc()->pgtbl,(uint64)&ch,addr+i,1);
        pipe->data[pipe->nwrite++ % PIPE_SIZE] = ch;
    }
    proc_wakeup(&pipe->nread);
    spinlock_release(&pipe->lock);
    return i;
}

// 写入一个管道
uint32 pipe_read(pipe_t *pipe, uint64 addr, uint32 len)
{
    assert(pipe!=NULL,"pipe_read: Invalid pipe pointer");

    int i=0;
    char ch;
    if(len==0)
        return 0;

    spinlock_acquire(&pipe->lock);
    while(pipe->nread==pipe->nwrite && pipe->writeopen)
    {
        proc_sleep(&pipe->nread,&pipe->lock);
    }
    for(i=0;i<len;i++)
    {
        if(pipe->nread==pipe->nwrite)
        {
            break;
        }
        ch=pipe->data[pipe->nread++ % PIPE_SIZE];
        uvm_copyout(myproc()->pgtbl,addr+i,(uint64)&ch,1);
    }
    proc_wakeup(&pipe->nwrite);
    spinlock_release(&pipe->lock);
    return i;
}

// 关闭一个管道
void pipe_close(pipe_t *pipe, int writable)
{
    spinlock_acquire(&pipe->lock);
    if(writable)
    {
        pipe->writeopen=0;
        proc_wakeup(&pipe->nread);
    }
    else
    {
        pipe->readopen=0;
        proc_wakeup(&pipe->nwrite);
    }
    spinlock_release(&pipe->lock);
    if(!pipe->readopen && !pipe->writeopen)
    {
        pmem_free((uint64)pipe,1);
    }
    spinlock_release(&pipe->lock);
}
