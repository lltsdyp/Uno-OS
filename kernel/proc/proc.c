#include "lib/print.h"
#include "lib/str.h"
#include "mem/pmem.h"
#include "mem/vmem.h"
#include "proc/cpu.h"
#include "proc/initcode.h"
#include "memlayout.h"
#include "mem/mmap.h"
#include "lib/lock.h"
#include "proc/proc.h"
#include "riscv.h"
#include "fs/fs.h"
#include "dev/timer.h"
#include "fs/dir.h"

#define RATE_GRADIENT 8

// in trampoline.S
extern char trampoline[];

// in swtch.S
extern void swtch(context_t *old, context_t *new);

// in trap_user.c
extern void trap_user_return();

// 内核页表
extern pgtbl_t kernel_pgtbl;

// 进程数组
static proc_t procs[NPROC];

// 第一个进程的指针
static proc_t* proczero;

// 分配到哪一个pid了？
static int global_pid = 1;
static spinlock_t lk_pid;

// 申请一个pid(锁保护)
static int alloc_pid()
{
    int tmp = 0;
    spinlock_acquire(&lk_pid);
    assert(global_pid >= 0, "alloc_pid: overflow");
    tmp = global_pid++;
    spinlock_release(&lk_pid);
    return tmp;
}

// 释放锁 + 调用 trap_user_return
static void fork_return()
{
    // TODO:修改
    // 由于调度器中上了锁，所以这里需要解锁
    proc_t* p = myproc();
    spinlock_release(&p->lk);

    // 仅在第一个进程初始化文件系统
    if (p->pid == 1) {
        fs_init();  // 初始化文件系统
        p->cwd=path_to_inode("/");
        p->filelist[0]=file_create_dev("console",DEV_CONSOLE,0);
        p->filelist[1]=file_dup(p->filelist[0]);
    }

    trap_user_return();
}

// 获得一个初始化过的用户页表
// 完成了trapframe 和 trampoline 的映射
pgtbl_t proc_pgtbl_init(uint64 trapframe)
{
    pgtbl_t pgtbl = pmem_alloc(true);
    memset(pgtbl, 0, PGSIZE);
    vm_mappages(pgtbl, TRAMPOLINE, (uint64)trampoline, TRAMPOLINE_SIZE, PTE_X | PTE_R);
    vm_mappages(pgtbl, TRAPFRAME, trapframe, PGSIZE, PTE_W | PTE_R);

    return pgtbl;
}

// 将initcode加载到内存中
// pgtbl:给定的用户页表
static void load_initcode(pgtbl_t pgtbl)
{
    uint64 addr=0;

    // data + code 映射
    assert(initcode_len <= PGSIZE, "proc_make_first: initcode too big\n");

    // pgtbl不可能是kernel_pgtbl
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
    // 分配PCB字段
    proc_t *first_proc=proc_alloc();    
    // ustack 映射 + 设置 ustack_pages
    for (int i = 0; i < USER_STACK_INITIAL_PAGE_COUNT; ++i)
    {
        vm_mappages(first_proc->pgtbl, PGROUNDDOWN(USER_STACK_BOTTOM - (i+1) * PGSIZE),
                    (uint64)pmem_alloc(false), PGSIZE, PTE_W | PTE_R | PTE_U);
    }

    // 加载程序
    load_initcode(first_proc->pgtbl);

    // 设置 heap_top
    first_proc->heap_top = USER_VMEM_START + PGSIZE;

    // tf字段设置
    first_proc->tf->epc = (uint64)USER_VMEM_START;
    first_proc->tf->sp= USER_STACK_BOTTOM;


    // 设置mmap区域
    first_proc->mmap=mmap_region_alloc(false);
    first_proc->mmap->begin=MMAP_BEGIN;
    first_proc->mmap->npages=0;//特殊标记节点，表示开始
    first_proc->mmap->next=mmap_region_alloc(true);

    first_proc->parent=NULL;
    first_proc->state=RUNNABLE;
    first_proc->begin_runnable_time=timer_get_ticks();
    proczero=first_proc;

    spinlock_release(&(first_proc->lk));
}

// 返回一个未使用的进程空间
// 设置pid + 设置上下文中的ra和sp
// 申请tf和pgtbl使用的物理页
// alloc分配的进程需要使用spinlock_release解锁
proc_t* proc_alloc()
{
    int pid=alloc_pid();
    proc_t *newproc=NULL;

    // 能否使用哈希优化？
    for(int i=0;i<NPROC;++i)
    {
        newproc=&procs[i];
        spinlock_acquire(&(newproc->lk));
        if(newproc->state==UNUSED)
        {
            break;
        }
        else
        {
            spinlock_release(&(newproc->lk));
            newproc=NULL;
        }
    }
    assert(newproc!=NULL, "proc_alloc: no proc available");

    // pid初始化
    newproc->pid=pid;

    // pagetable相关的初始化
    newproc->tf=(trapframe_t *)pmem_alloc(false);
    newproc->ustack_pages=USER_STACK_INITIAL_PAGE_COUNT;
    newproc->pgtbl=proc_pgtbl_init((uint64)newproc->tf);


    // 准备上下文切换
    memset(&(newproc->ctx),0,sizeof(newproc->ctx));
    newproc->ctx.ra=(uint64)fork_return;
    newproc->ctx.sp=newproc->kstack+PGSIZE;

    // 注意到正常结束时进程锁并未被释放，这样，我们在调用alloc创建一个新进程后不要忘了释放这个锁
    // 这是为了防止我们的进程在创建过程中被意外地调度
    return newproc;
 
}

// 释放一个进程空间
// 释放pgtbl的整个地址空间
// 释放mmap_region到仓库
// 设置其余各个字段为合适初始值
// tips: 调用者需持有p->lk
void proc_free(proc_t* p)
{
    if(p->tf!=NULL)
    {
        pmem_free((uint64)(p->tf),0);
    }
    // 释放mmap区域
    mmap_region_t *region=p->mmap;
    while(region!=NULL)
    {
        mmap_region_t *prev_region=region;
        region=region->next;
        mmap_region_free(prev_region);
    }
    // 可能需要清空页表
    if(p->pgtbl!=NULL){
        vm_unmappages(p->pgtbl, TRAPFRAME, PGSIZE, false);
        vm_unmappages(p->pgtbl, TRAMPOLINE, PGSIZE, false);
        uvm_destroy_pgtbl(p->pgtbl,3);
    }

    // 清空所有字段
    p->pid=-1;
    p->state=UNUSED;
    p->parent=NULL;
    p->exit_state=0;
    p->sleep_space=NULL;

    p->pgtbl=NULL;
    p->heap_top=0;
    p->ustack_pages=0;
    p->tf=NULL;
    p->mmap=NULL;
    
    p->begin_runnable_time=0;
    p->begin_running_time=0;
    p->total_exec_time=0;
    p->total_wait_time=0;

    memset((void *)&(p->ctx),0,sizeof(context_t));
}

// 进程模块初始化
void proc_init()
{
    spinlock_init(&lk_pid, "pid");
    for(uint32 i=0;i<NPROC;++i)
    {
        spinlock_init(&procs[i].lk, "proc");
        proc_free(&procs[i]);
        // kstack初始化
        procs[i].kstack=KSTACK(i);
        vm_mappages(kernel_pgtbl,procs[i].kstack,(uint64)pmem_alloc(true),PGSIZE,PTE_R|PTE_W);
    }
}

// 进程复制
// UNUSED -> RUNNABLE
int proc_fork()
{
    proc_t* new_proc=proc_alloc();
    assert(new_proc!=NULL, "proc_fork: no proc available");

    // 设置子进程进入用户态后的状态
    // 子进程中调用fork的返回值储存在a0中，为0
    *(new_proc->tf)=*(myproc()->tf);
    new_proc->tf->a0=0;

    // 文件描述符的拷贝
    for(int i=0;i<FILE_PER_PROC;++i)
    {
        if(myproc()->filelist[i]!=NULL)
            new_proc->filelist[i]=file_dup(myproc()->filelist[i]);
    }
    new_proc->cwd=inode_dup(myproc()->cwd);

    // 拷贝页表
    uvm_copy_pgtbl(myproc()->pgtbl,new_proc->pgtbl,myproc()->heap_top,myproc()->ustack_pages,myproc()->mmap);

    new_proc->mmap=mmap_region_alloc(false);
    mmap_region_t *new_region=new_proc->mmap;
    // 拷贝mmap链
    for(mmap_region_t *region=myproc()->mmap;region!=NULL;region=region->next)
    {
        new_region->begin=region->begin;
        new_region->npages=region->npages;
        if(region->next!=NULL){
            new_region->next=mmap_region_alloc(false);
        }
        else{
            new_region->next=NULL;
        }
        new_region=new_region->next;
    }

    // 设置pcb中的其他字段
    new_proc->heap_top=myproc()->heap_top;
    new_proc->ustack_pages=myproc()->ustack_pages;

    // 进程状态设置
    new_proc->state=RUNNABLE;
    new_proc->begin_runnable_time=timer_get_ticks();
    new_proc->parent=myproc();

    spinlock_release(&(new_proc->lk));

    return new_proc->pid;
}

// 进程放弃CPU的控制权
// RUNNING -> RUNNABLE
void proc_yield()
{
    spinlock_acquire(&(myproc()->lk));
    myproc()->state=RUNNABLE;
    uint64 ticks=timer_get_ticks();
    myproc()->total_exec_time+=ticks-myproc()->begin_running_time;
    myproc()->begin_runnable_time=ticks;
    proc_sched();
    spinlock_release(&(myproc()->lk));
}

// 等待一个子进程进入 ZOMBIE 状态
// 将退出的子进程的exit_state放入用户给的地址 addr
// 成功返回子进程pid，失败返回-1
int proc_wait(uint64 addr)
{
    // 避免丢失子进程退出的信息
    spinlock_acquire(&(myproc()->lk));

    int have_kids=0;// 在第一次轮询中，我们需要检查该进程是否存在子进程，如果不检查的话，wait可能会无限等待下去，
    // 不停地轮询,直到有子进程进入ZOMBIE状态
    while(1)
    {
        have_kids=0;
        for(int idx=0;idx<NPROC;++idx)
        {
            if(procs[idx].parent==myproc())
            {
                have_kids=1;
                // 同样的，放在if内部防止死锁的出现
                spinlock_acquire(&(procs[idx].lk));

                if(procs[idx].state==ZOMBIE)
                {
                    int pid=procs[idx].pid;
                    // 保存退出时的状态
                    uvm_copyout(myproc()->pgtbl,addr,(uint64)&(procs[idx].exit_state),sizeof(procs[idx].exit_state));

                    proc_free(&(procs[idx]));
                    spinlock_release(&(myproc()->lk));
                    spinlock_release(&(procs[idx].lk));
                    return pid;
                }
                spinlock_release(&(procs[idx].lk));
            }
        }

        if(!have_kids)
        {
            return -1;
        }

        proc_sleep(myproc(),&(myproc()->lk));
    }
}


// 父进程退出，子进程认proczero做父，因为它永不退出
static void proc_reparent(proc_t* parent)
{
    for(int idx=0;idx<NPROC;++idx)
    {
        if(procs[idx].parent==parent)
        {
            // 锁的获取操作放在if语句内部以避免死锁的产生
            spinlock_acquire(&(procs[idx].lk));
            procs[idx].parent=proczero;
            spinlock_release(&(procs[idx].lk));
        }
    }
}

// 唤醒一个进程
static void proc_wakeup_one(proc_t* p)
{
    assert(spinlock_holding(&p->lk), "proc_wakeup_one: lock");
    if(p->state == SLEEPING && p->sleep_space == p) {
        p->state = RUNNABLE;
    }
}

// 进程退出
void proc_exit(int exit_state)
{
    assert(myproc()->pid!=1,"proc_exit: proczero exited!");

    // 保证proczero可以接收当前进程，首先将其唤醒
    spinlock_acquire(&(proczero->lk));
    proc_wakeup_one(proczero);
    spinlock_release(&(proczero->lk));

    // 然后获取父进程
    spinlock_acquire(&(myproc()->lk));
    proc_t *parent=myproc()->parent;
    spinlock_release(&(myproc()->lk));

    // 将父进程和子进程均上锁，进行下一步操作
    spinlock_acquire(&(parent->lk));
    spinlock_acquire(&(myproc()->lk));

    proc_reparent(myproc());

    proc_wakeup_one(parent);

    myproc()->exit_state=exit_state;
    myproc()->state=ZOMBIE;

    spinlock_release(&(parent->lk));

    proc_sched();
    panic("Exit failed!");
}

// 进程切换到调度器
// ps: 调用者保证持有当前进程的锁
void proc_sched()
{
    assert(spinlock_holding(&(myproc()->lk)), "proc_sched: Not holding lock");
    assert(mycpu()->noff==1, "proc_sched: mycpu()->noff!=1");
    assert(myproc()->state!=RUNNING, "proc_sched: proc is running");
    assert(intr_get()==0,"proc_sched: interruptible");

    // 切换上下文，
    int origin=mycpu()->origin;
    swtch(&(myproc()->ctx),&(mycpu()->ctx));
    mycpu()->origin=origin;
}

// 检查响应比
// 当前锁被持有
static uint64 get_weight(proc_t *p)
{
    assert(spinlock_holding(&(p->lk)), "get_weight: Not holding lock");
    if(p->total_exec_time==0)
    {
        return -1;// -1等价于UINT64_MAX
    }
    return ((p->total_exec_time+p->total_wait_time)<<RATE_GRADIENT)/p->total_exec_time;
}

// 调度器
void proc_scheduler()
{
    mycpu()->proc = NULL; // 当前没有正在执行的进程
    int found = 0;
    proc_t *p = NULL,*next_p = NULL;
    while (1)
    {
        // 开中断以避免死锁的发生
        intr_on();

        found = 0;
        next_p=NULL;
        uint64 max_weight=0;
        uint64 ticks = timer_get_ticks();
        for (int idx = 0; idx < NPROC; ++idx)
        {
            p = &procs[idx];
            spinlock_acquire(&(p->lk));
            if (p->state == RUNNABLE)
            {
                p->total_wait_time += ticks - p->begin_runnable_time; // 首先更新他们的等待时间，然后再进行权重计算
                p->begin_runnable_time=ticks;

                // 下一个要执行的进程就是当前我们检查的proc
                if(max_weight<get_weight(p))
                {
                    max_weight=get_weight(p);
                    next_p=p;
                }

                found = 1;
            }
            spinlock_release(&(p->lk));
        }

        if (found == 0)
        {
            intr_on();
            asm volatile("wfi"); // 进入低功耗模式等待可执行的进程
        }
        else
        {
            spinlock_acquire(&(next_p->lk));
            assert(next_p->state == RUNNABLE, "proc_scheduler: proc is not runnable");
            next_p->state = RUNNING;
            mycpu()->proc = next_p;
            next_p->begin_running_time = ticks;                        // 记录开始运行的时间

            // 切换到next_p执行
            swtch(&(mycpu()->ctx), &(next_p->ctx));

            // 更新进程的运行时间
            ticks = timer_get_ticks();
            mycpu()->proc->total_exec_time += ticks - mycpu()->proc->begin_running_time; // 累计执行时间
            mycpu()->proc->begin_runnable_time = ticks;                                  // 记录等待的开始时间
            spinlock_release(&(next_p->lk));

            // 发生进程调度前首先将cpu的当前proc清空
            mycpu()->proc = NULL;
        }
    }
}

// 进程睡眠在sleep_space
void proc_sleep(void* sleep_space, spinlock_t* lk)
{
    // 传入时的lk并不保证是当前进程的锁，那么我们要将传入的锁的开关转换为当前锁的开关
    if(lk!=&(myproc()->lk))
    {
        spinlock_acquire(&(myproc()->lk));
        spinlock_release(lk);
    }

    myproc()->sleep_space=sleep_space;
    myproc()->state=SLEEPING;
    uint64 ticks=timer_get_ticks();
    myproc()->total_exec_time+=ticks-myproc()->begin_running_time;

    proc_sched();

    // 直到调用了wakeup后，才会重新被调度，这时当前进程的状态已经被设置为RUNNABLE了。
    myproc()->sleep_space=NULL;

    // 可能需要恢复之前的锁状态
    if(lk!=&(myproc()->lk))
    {
        spinlock_release(&(myproc()->lk));
        spinlock_acquire(lk);
    }
}

// 唤醒所有在sleep_space沉睡的进程
void proc_wakeup(void* sleep_space)
{
    for(int idx=0;idx<NPROC;++idx)
    {
        proc_t* p=&procs[idx];
        spinlock_acquire(&(p->lk));
        if(p->state==SLEEPING && p->sleep_space==sleep_space)
        {
            p->state=RUNNABLE;
            p->begin_runnable_time=timer_get_ticks();
        }
        spinlock_release(&(p->lk));
    }
}

// TODO
