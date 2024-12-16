#ifndef __CONSOLE_H__
#define __CONSOLE_H__

#include "lib/lock.h"

#define CONSOLE_INPUT_BUF    64
#define CONSOLE_OUTPUT_BUF   64

typedef struct console{
    spinlock_t lk;
    char buf[CONSOLE_INPUT_BUF];
    uint32 read_idx;
    uint32 write_idx;
    uint32 edit_idx;
} console_t;

uint32 console_read(uint32 len, uint64 dst, bool user);
uint32 console_write(uint32 len, uint64 src, bool user);
void   console_intr(int c);
void   console_init();

#endif