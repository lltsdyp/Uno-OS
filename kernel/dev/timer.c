#include "lib/lock.h"
#include "lib/print.h"
#include "dev/timer.h"
#include "memlayout.h"
#include "riscv.h"

/*-------------------- 工作在M-mode --------------------*/

// in trap.S M-mode时钟中断处理流程()
extern void timer_vector();


// 每个CPU在时钟中断中需要的临时空间(考虑为什么可以这么写)
static uint64 mscratch[NCPU][5];

// 时钟初始化
// called in start.c
void timer_init()
{
    int current_cpuid=r_mhartid();

    // 时钟中断的频率由TIMER_INTERVAL的值指定
    *(uint64*)CLINT_MTIMECMP(current_cpuid)=*(uint64*)CLINT_MTIME+TIMER_INTERVAL;

    // 接下来的目标：将时钟中断处理函数timer_vector的位置写入mtvec中以及将时钟中断相关参数写入mscratch中
    // 同名的寄存器中会存储指向该数组的指针
    // trap.S中我们已经说明了mscratch数组中五个元素的意义：
    // 0..2 分别存储了a1,a2,a3寄存器的值（起暂存作用）
    // 3 存储着CLINT_MTIMECMP的地址
    // 4 存储TIMER_INTERVAL
    mscratch[current_cpuid][3]=CLINT_MTIMECMP(current_cpuid);
    mscratch[current_cpuid][4]=TIMER_INTERVAL;
    w_mscratch((uint64)mscratch[current_cpuid]);
    w_mtvec((uint64)timer_vector);

    // 设置允许M模式下的时钟中断
    w_mstatus(r_mstatus() | MSTATUS_MIE);
    w_mie(r_mie()|MIE_MTIE);

}


/*--------------------- 工作在S-mode --------------------*/

// 系统时钟
static timer_t sys_timer;

// 时钟创建(初始化系统时钟)
void timer_create()
{
    spinlock_init(&sys_timer.lk, "sys_timer");
}

// 时钟更新(ticks++ with lock)
void timer_update()
{
    spinlock_acquire(&sys_timer.lk);
    sys_timer.ticks++;
    spinlock_release(&sys_timer.lk);
}

// 返回系统时钟ticks
uint64 timer_get_ticks()
{
    uint64 current_tick= sys_timer.ticks;
    return current_tick;
}
