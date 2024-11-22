#include "lib/lock.h"
#include "lib/print.h"
#include "proc/cpu.h"

void sleeplock_init(sleeplock_t* lk, char* name)
{
    spinlock_init(&(lk->lk),"sleeplock");
    lk->name=name;
    lk->locked=0;
    lk->pid=SLEEPLOCK_INVALID_PID;
}

void sleeplock_acquire(sleeplock_t* lk)
{
    spinlock_acquire(&(lk->lk));
    while(lk->locked){
        proc_sleep(lk,&(lk->lk));
    }
    lk->locked=1;
    lk->pid=myproc()->pid;
    spinlock_release(&(lk->lk));
}

void sleeplock_release(sleeplock_t* lk)
{
    spinlock_acquire(&(lk->lk));
    lk->locked=0;
    lk->pid=SLEEPLOCK_INVALID_PID;
    proc_wakeup(lk);
    spinlock_release(&(lk->lk));
}

bool sleeplock_holding(sleeplock_t* lk)
{
    int holding=0;

    spinlock_acquire(&(lk->lk));
    holding=lk->locked && (lk->pid==myproc()->pid);
    spinlock_release(&(lk->lk));
    return holding;
}
