#include "lib/print.h"
#include "syscall/syscall.h"
#include "syscall/sysfunc.h"
#include "proc/cpu.h"
#include "proc/proc.h"
#include "memlayout.h"

uint64 sys_halt()
{
    printf("Shutdown system...\n");
    *((volatile uint32*)SHUTDOWN_REG) = 0x5555;
    return 0;
}

uint64 sys_reboot()
{
    printf("Reboot system...\n");
    pid_reset();
    proc_reset();
    printf("Done!\n");
    return 0;
}
