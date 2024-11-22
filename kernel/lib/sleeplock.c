#include "lib/lock.h"
#include "lib/print.h"
#include "proc/cpu.h"

void sleeplock_init(sleeplock_t* lk, char* name)
{
    printf("TODO: sleeplock_init\n");
}

void sleeplock_acquire(sleeplock_t* lk)
{
    printf("TODO: sleeplock_acquire\n");
}

void sleeplock_release(sleeplock_t* lk)
{
    printf("TODO: sleeplock_release\n");
}

bool sleeplock_holding(sleeplock_t* lk)
{
    printf("TODO: sleeplock_holding\n");
    return 0;
}
