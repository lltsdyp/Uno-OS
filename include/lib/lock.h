#ifndef __LOCK_H__
#define __LOCK_H__

#include "common.h"

typedef struct spinlock {
    int locked;
    char* name;
    int cpuid;
} spinlock_t;

typedef struct sleeplock {
    spinlock_t lk;
    int locked;
    char* name;
    int pid;
} sleeplock_t;

#define SPINLOCK_INVALID_CPUID (-1)

void push_off();
void pop_off();

void spinlock_init(spinlock_t* lk, char* name);
void spinlock_acquire(spinlock_t* lk);
void spinlock_release(spinlock_t* lk);
bool spinlock_holding(spinlock_t* lk); 

void sleeplock_init(sleeplock_t* lk, char* name);
void sleeplock_acquire(sleeplock_t* lk);
void sleeplock_release(sleeplock_t* lk);
bool sleeplock_holding(sleeplock_t* lk);

#endif