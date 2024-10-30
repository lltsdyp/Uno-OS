#ifndef __TESTCASES_H__
#define __TESTCASES_H__

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

volatile static int started = 0;

extern int main();

#ifdef LAB_1_CASE_1

int main()
{
    // int val1 = 1;
    // int val2 = 2;
    // int val3 = 3;
    // TODO:进行系统的部分初始化
    // 这里先打印一个字符，代表进入了main函数
    if (mycpuid() == 0)
    {
        print_init();
        // 调试信息
        // printf("hart %d starting\n", mycpuid());
        // panic("%d %d %d\n", val1, val2, val3);
        printf("%p\n",0x3fffffe000);
        started=1;
    }
    else
    {
        //等待cpu0完成所有启动所需的初始化工作
        while (!started)
            ;
    }
    printf("hart %d starting\n", mycpuid());
    while (1)
        ;
}

#elif defined LAB_2_CASE_1

volatile static int over_1 = 0, over_2 = 0;

static int* mem[1024];

int main()
{
    int cpuid = r_tp();

    if(cpuid == 0) {

        print_init();
        pmem_init();

        printf("cpu %d is booting!\n", cpuid);
        __sync_synchronize();
        started = 1;

        for(int i = 0; i < 512; i++) {
            mem[i] = pmem_alloc(true);
            memset(mem[i], 1, PGSIZE);
            printf("mem = %p, data = %d\n", mem[i], mem[i][0]);
        }
        printf("cpu %d alloc over\n", cpuid);
        over_1 = 1;
        
        while(over_1 == 0 || over_2 == 0);
        
        for(int i = 0; i < 512; i++)
            pmem_free((uint64)mem[i], true);
        printf("cpu %d free over\n", cpuid);

    } else {

        while(started == 0);
        __sync_synchronize();
        printf("cpu %d is booting!\n", cpuid);
        
        for(int i = 512; i < 1024; i++) {
            mem[i] = pmem_alloc(true);
            memset(mem[i], 1, PGSIZE);
            printf("mem = %p, data = %d\n", mem[i], mem[i][0]);
        }
        printf("cpu %d alloc over\n", cpuid);
        over_2 = 1;

        while(over_1 == 0 || over_2 == 0);

        for(int i = 512; i < 1024; i++)
            pmem_free((uint64)mem[i], true);
        printf("cpu %d free over\n", cpuid);        
 
    }
    while (1);    
}


#elif defined LAB_2_CASE_2

int main()
{
    int cpuid = r_tp();

    if(cpuid == 0) {

        print_init();
        pmem_init();
        kvm_init();
        kvm_inithart();
        printf("cpu %d is booting!\n", cpuid);

        __sync_synchronize();
        // started = 1;

        pgtbl_t test_pgtbl = pmem_alloc(true);
        uint64 mem[5];
        for(int i = 0; i < 5; i++)
            mem[i] = (uint64)pmem_alloc(false);

        printf("\ntest-1\n\n");    
        vm_mappages(test_pgtbl, 0, mem[0], PGSIZE, PTE_R);
        vm_mappages(test_pgtbl, PGSIZE * 10, mem[1], PGSIZE / 2, PTE_R | PTE_W);
        vm_mappages(test_pgtbl, PGSIZE * 512, mem[2], PGSIZE - 1, PTE_R | PTE_X);
        vm_mappages(test_pgtbl, PGSIZE * 512 * 512, mem[2], PGSIZE, PTE_R | PTE_X);
        vm_mappages(test_pgtbl, VA_MAX - PGSIZE, mem[4], PGSIZE, PTE_W);
        vm_print(test_pgtbl);

        printf("\ntest-2\n\n");    
        vm_mappages(test_pgtbl, 0, mem[0], PGSIZE, PTE_W);
        vm_unmappages(test_pgtbl, PGSIZE * 10, PGSIZE, true);
        vm_unmappages(test_pgtbl, PGSIZE * 512, PGSIZE, true);
        vm_print(test_pgtbl);

    } else {

        while(started == 0);
        __sync_synchronize();
        printf("cpu %d is booting!\n", cpuid);
         
    }
    while (1);    
}

#elif defined LAB_3_CASE_1

int main()
{
    intr_off();
    if (mycpuid() == 0)
    {
        print_init();
        pmem_init();
        kvm_init();
        kvm_inithart();
        trap_kernel_init();
        trap_kernel_inithart();
        plic_init();
        plic_inithart();
        __sync_synchronize();
        started=1;
    }
    else
    {
        //等待cpu0完成所有启动所需的初始化工作
        while (!started)
            ;
        __sync_synchronize();
        kvm_inithart();
        trap_kernel_inithart();
        plic_inithart();
    }
    printf("hart %d starting\n", mycpuid());
    while (1)
        ;
}

#elif defined LAB_4_CASE_1

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
        plic_init();
        plic_inithart();

        proc_make_first();
        
        __sync_synchronize();
        // started = 1;

    } else {

        while(started == 0);
        __sync_synchronize();
        printf("cpu %d is booting!\n", cpuid);
        kvm_inithart();
        trap_kernel_inithart();
        plic_inithart();
    }
    printf("Ready\n");
    while (1);    
}

#else 

#error "No pre-defined macro. Can not decide what to test or run"

#endif

#endif // __TESTCASES_H__
