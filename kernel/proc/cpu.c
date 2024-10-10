#include "proc/cpu.h"
#include "riscv.h"

static cpu_t cpus[NCPU];

cpu_t* mycpu(void)
{
    return &cpus[mycpuid()];
}

int mycpuid(void) 
{
    uint64 id=r_tp();
    return (int)id;
}
