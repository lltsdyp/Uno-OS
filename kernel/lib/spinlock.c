#include "lib/lock.h"
#include "lib/print.h"
#include "proc/cpu.h"
#include "riscv.h"

// 带层数叠加的关中断
void push_off(void)
{
    mycpu()->noff++;
    int interrupt_status=intr_get();

    // 层数大于等于1，说明需要关闭中断
    if(mycpu()->noff>=1)
    {
        intr_off();
        if(mycpu()->noff==1)
        {
            mycpu()->origin=interrupt_status;
        }
    }
}

// 带层数叠加的开中断
void pop_off(void)
{
    int interrupt_status=intr_get();

    assert(interrupt_status==0, "spinlock_pop_off: interruptible");
    assert(mycpu()->noff>=0, "spinlock_pop_off: no interrupts were disabled!");
    mycpu()->noff--;
    if(mycpu()->noff==0)
    {
        intr_on();
    }
}

// 是否持有自旋锁
// 中断应当是关闭的
bool spinlock_holding(spinlock_t *lk)
{
    return (lk->cpuid==mycpuid() && lk->locked);
}

// 自旋锁初始化
void spinlock_init(spinlock_t *lk, char *name)
{
    lk->cpuid=SPINLOCK_INVALID_CPUID;// 自旋锁的cpuid为-1时代表该自旋锁没有被关联到任何cpu
    lk->locked=0;
    lk->name=name;
}

// 获取自选锁
void spinlock_acquire(spinlock_t *lk)
{
    push_off(); //关中断

    // 如果当前已经持有了自旋锁的话，再次请求该自旋锁显然是不合理的，故报错
    assert(!spinlock_holding(lk), "spinlock_acquire: reacquiring lock %s",lk->name);

    // 进入忙等状态，直到获取自旋锁
    while(__sync_lock_test_and_set(&lk->locked, 1) != 0)
        ;

    // 保证内存访问存取的一致性
    __sync_synchronize();

    lk->cpuid=mycpuid();
    
} 

// 释放自旋锁
void spinlock_release(spinlock_t *lk)
{
    // 如果当前暂未持有自旋锁，则报错
    assert(spinlock_holding(lk), "spinlock_release: lock is not holding %s",lk->name);

    lk->cpuid=SPINLOCK_INVALID_CPUID;

    __sync_synchronize();

    __sync_lock_release(&lk->locked);

    pop_off();
}