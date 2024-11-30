#ifndef __SYSFUNC_H__
#define __SYSFUNC_H__

#include "common.h"

uint64 sys_print();
uint64 sys_brk();
uint64 sys_mmap();
uint64 sys_munmap();
uint64 sys_fork();
uint64 sys_wait();
uint64 sys_exit();
uint64 sys_sleep();
uint64 sys_alloc_block();
uint64 sys_free_block();
uint64 sys_read_block();
uint64 sys_write_block();
uint64 sys_release_block();
uint64 sys_show_buf();

#endif