#ifndef __SEMAPHORE_H__
#define __SEMAPHORE_H__

#include "common.h"
#include "lib/lock.h"

#define NSEM 16

typedef struct semaphore {
    spinlock_t lk;
    int value;
    int valid;
} semaphore_t;

#endif