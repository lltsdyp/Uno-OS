#ifndef __SYSFUNC_H__
#define __SYSFUNC_H__

#include "common.h"

// in sysproc.c
uint64 sys_exec();
uint64 sys_brk();
uint64 sys_mmap();
uint64 sys_munmap();
uint64 sys_fork();
uint64 sys_wait();
uint64 sys_exit();
uint64 sys_sleep();
uint64 sys_pid();
uint64 sys_ppid();
uint64 sys_time();

// in sysfile.c
uint64 sys_open();
uint64 sys_close();
uint64 sys_read();
uint64 sys_write();
uint64 sys_lseek();
uint64 sys_dup();
uint64 sys_fstat();
uint64 sys_getdir();
uint64 sys_mkdir();
uint64 sys_chdir();
uint64 sys_link();
uint64 sys_unlink();
uint64 sys_pipe();

// in sysconsole.c
uint64 sys_curhide();
uint64 sys_curshow();
uint64 sys_backcolor();
uint64 sys_forecolor();
uint64 sys_clear();
uint64 sys_conreset();

// in syspower.c
uint64 sys_reboot();
uint64 sys_halt();

#endif