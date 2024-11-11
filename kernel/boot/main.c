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

// volatile static int started = 0;

// extern int main();
// int main()
// {
//     int cpuid = r_tp();

//     if(cpuid == 0) {
        
//         print_init();
//         printf("cpu %d is booting!\n", cpuid);
//         pmem_init();
//         kvm_init();
//         kvm_inithart();
//         trap_kernel_init();
//         trap_kernel_inithart();        
//         mmap_init();

//         proc_make_first(); // 生成第一个进程，以后修改测试用例在initcode.c中进行

//         __sync_synchronize();
//         started = 1;

//     } else {

//         while(started == 0);
//         __sync_synchronize();
        
//         printf("cpu %d is booting!\n", cpuid);
//         kvm_inithart();
//         trap_kernel_inithart();
//     }
 
//     while (1);
// }

volatile static int started = 0;
volatile static bool over_1 = false, over_2 = false;
volatile static bool over_3 = false, over_4 = false;

void *mmap_list[63];

int main()
{
    int cpuid = r_tp();

    if (cpuid == 0)
    {

        print_init();
        printf("cpu %d is booting!\n", cpuid);
        pmem_init();
        kvm_init();
        kvm_inithart();
        trap_kernel_init();
        trap_kernel_inithart();

        // 初始化 + 初始状态显示
        mmap_init();
        mmap_show_mmaplist();
        printf("\n");

        __sync_synchronize();
        started = 1;

        // 申请
        for (int i = 0; i < 32; i++)
            mmap_list[i] = mmap_region_alloc();
        over_1 = true;

        // 屏障
        while (over_1 == false || over_2 == false)
            ;

        // 释放
        for (int i = 0; i < 32; i++)
            mmap_region_free(mmap_list[i]);
        over_3 = true;

        // 屏障
        while (over_3 == false || over_4 == false)
            ;

        // 查看结束时的状态
        mmap_show_mmaplist();
    }
    else
    {

        while (started == 0)
            ;
        __sync_synchronize();
        printf("cpu %d is booting!\n", cpuid);
        kvm_inithart();
        trap_kernel_inithart();

        // 申请
        for (int i = 32; i < 63; i++)
            mmap_list[i] = mmap_region_alloc();
        over_2 = true;

        // 屏障
        while (over_1 == false || over_2 == false)
            ;

        // 释放
        for (int i = 32; i < 63; i++)
            mmap_region_free(mmap_list[i]);
        over_4 = true;
    }

    while (1)
        ;
}