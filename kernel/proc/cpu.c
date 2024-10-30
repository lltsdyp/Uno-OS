#include "proc/cpu.h"
#include "riscv.h"

static cpu_t cpus[NCPU];

void push_off();
void pop_off();

cpu_t* mycpu(void)
{
    return &cpus[mycpuid()];
}

int mycpuid(void) 
{
    uint64 id=r_tp();
    return (int)id;
}

proc_t* myproc(void)
{
    push_off();
    cpu_t* current_cpu=mycpu();
    proc_t* current_proc=current_cpu->proc;
    pop_off();
    return current_proc;
}
