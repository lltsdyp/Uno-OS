// 由于后期积累的测试样例过多不便于调试
// 故将所有的测试用例放在 boot/testcases.h 中
// 通过宏定义来决定使用哪个测试用例


#include "riscv.h"
#include "lib/print.h"
#include "lib/str.h"
#include "mem/pmem.h"
#include "mem/vmem.h"
#include "mem/mmap.h"
#include "dev/vio.h"
#include "proc/proc.h"
#include "trap/trap.h"
#include "dev/plic.h"
#include "dev/console.h"
#include "fs/buf.h"
#include "fs/inode.h"

volatile static int started = 0;

int main()
{
    int cpuid = r_tp();

    if(cpuid == 0) {
        console_init();
        print_init();
        pmem_init();
        kvm_init();
        kvm_inithart();
        trap_kernel_init();
        trap_kernel_inithart();
        plic_init();
        plic_inithart();
        mmap_init();
        buf_init();
        inode_init();
        virtio_disk_init();
        proc_init();
        proc_make_first();

        printf("cpu %d is booting!\n", cpuid);
        __sync_synchronize();
        started = 1;
    } else {

        while(started == 0);
        __sync_synchronize();
        
        printf("cpu %d is booting!\n", cpuid);
        kvm_inithart();
        trap_kernel_inithart();
    }
    proc_scheduler();

    while (1);
}
