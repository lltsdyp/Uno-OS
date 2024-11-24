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

volatile static int started = 0;

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
        virtio_disk_init();
        proc_init();
        proc_make_first();

        __sync_synchronize();
        // started = 1;
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

// ONLY FOR TEST5CASE3

// volatile static int started = 0;
// volatile static bool over_1 = false, over_2 = false;
// volatile static bool over_3 = false, over_4 = false;

// void *mmap_list[63];

// int main()
// {
//     int cpuid = r_tp();

//     if (cpuid == 0)
//     {

//         print_init();
//         printf("cpu %d is booting!\n", cpuid);
//         pmem_init();
//         kvm_init();
//         kvm_inithart();
//         trap_kernel_init();
//         trap_kernel_inithart();

//         // 初始化 + 初始状态显示
//         mmap_init();
//         mmap_show_mmaplist();
//         printf("\n");

//         __sync_synchronize();
//         started = 1;

//         // 申请
//         for (int i = 0; i < 32; i++)
//             mmap_list[i] = mmap_region_alloc();
//         over_1 = true;

//         // 屏障
//         while (over_1 == false || over_2 == false)
//             ;

//         // 释放
//         for (int i = 0; i < 32; i++)
//             mmap_region_free(mmap_list[i]);
//         over_3 = true;

//         // 屏障
//         while (over_3 == false || over_4 == false)
//             ;

//         // 查看结束时的状态
//         mmap_show_mmaplist();
//     }
//     else
//     {

//         while (started == 0)
//             ;
//         __sync_synchronize();
//         printf("cpu %d is booting!\n", cpuid);
//         kvm_inithart();
//         trap_kernel_inithart();

//         // 申请
//         for (int i = 32; i < 63; i++)
//             mmap_list[i] = mmap_region_alloc();
//         over_2 = true;

//         // 屏障
//         while (over_1 == false || over_2 == false)
//             ;

//         // 释放
//         for (int i = 32; i < 63; i++)
//             mmap_region_free(mmap_list[i]);
//         over_4 = true;
//     }

//     while (1)
//         ;
// }