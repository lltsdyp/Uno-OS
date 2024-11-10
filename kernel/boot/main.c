// 由于后期积累的测试样例过多不便于调试
// 故将所有的测试用例放在 boot/testcases.h 中
// 通过宏定义来决定使用哪个测试用例


#include "riscv.h"
#include "lib/print.h"
#include "dev/uart.h"
#include "proc/cpu.h"
#include "mem/pmem.h"
#include "mem/vmem.h"
#include "lib/str.h"
#include "trap/trap.h"
#include "dev/plic.h"
#include "proc/proc.h"
#include "mem/mmap.h"

volatile static int started = 0;

extern int main();
int main()
{
    int cpuid = r_tp();

    if(cpuid == 0) {
        
        print_init();
        printf("cpu %d is booting!\n", cpuid);
        pmem_init();
        kvm_init();
        kvm_inithart();
        trap_kernel_init();
        trap_kernel_inithart();        
        mmap_init();

        proc_make_first(); // 生成第一个进程，以后修改测试用例在initcode.c中进行

        __sync_synchronize();
        started = 1;

    } else {

        while(started == 0);
        __sync_synchronize();
        
        printf("cpu %d is booting!\n", cpuid);
        kvm_inithart();
        trap_kernel_inithart();
    }
 
    while (1);
}

