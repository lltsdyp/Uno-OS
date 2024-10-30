#include "lib/print.h"
#include "trap/trap.h"
#include "proc/cpu.h"
#include "mem/vmem.h"
#include "memlayout.h"
#include "riscv.h"
#include "trap/intrno.h"

// in trampoline.S
extern char trampoline[];  // 内核和用户切换的代码
extern char user_vector[]; // 用户触发trap进入内核
extern char user_return[]; // trap处理完毕返回用户

// in trap.S
extern char kernel_vector[]; // 内核态trap处理流程

// in trap_kernel.c
extern char *interrupt_info[16]; // 中断错误信息
extern char *exception_info[16]; // 异常错误信息

// 在user_vector()里面调用
// 用户态trap处理的核心逻辑
void trap_user_handler()
{
    // 这些寄存器保存了陷阱发生时的上下文信息。
    uint64 sepc = r_sepc();       // 记录了发生异常时的pc值
    uint64 sstatus = r_sstatus(); // 与特权模式和中断相关的状态信息
    uint64 scause = r_scause();   // 引发trap的原因
    // uint64 stval = r_stval();     // 发生trap时保存的附加信息(不同trap不一样)

    proc_t *p = myproc();
    int trap_id = scause & 0xf;

    // 确认trap来自U-mode
    assert((sstatus & SSTATUS_SPP) == 0, "trap_user_handler: not from u-mode");

    // 将中断和异常发送到 kerneltrap()
    w_stvec((uint64)kernel_vector);

    // 设置 S Exception Program Counter 为保存的用户 PC
    p->tf->epc = sepc;

    switch(trap_id)
    {
        // 系统调用
        case SMODE_SYSCALL_INTERRUPT:
        // sepc 指向 ecall 指令，但我们需要返回到下一条指令
            p->tf->epc += 4;
            intr_on();
            printf("get a syscall from proc %d\n", myproc()->pid);
            break;
        case SMODE_SOFTWARE_INTERRUPT:
            timer_interrupt_handler();
            break;
        case SMODE_EXTERNAL_INTERRUPT:
            external_interrupt_handler();
            break;
        default:
            // 说明发生中断
            if(scause & 0x8000000000000000L)
                panic("Unknown trap id %x,\n\tdescription:%s",trap_id,interrupt_info[trap_id]);
            // 发生异常
            else printf("Unknown trap id %x,\n\tdescription:%s", trap_id,exception_info[trap_id]);
    }

    // 返回用户态
    trap_user_return();
}

// 调用user_return()
// 内核态返回用户态
void trap_user_return()
{
    proc_t* p = myproc();
    intr_off();
    w_stvec(TRAMPOLINE+((uint64)user_vector-(uint64)trampoline));

    // 设置 uservec 需要的陷阱帧值，并恢复用户寄存器
    p->tf->kernel_satp = r_satp();         // 内核页表
    p->tf->kernel_sp = p->kstack + PGSIZE; // 进程的内核栈
    p->tf->kernel_trap = (uint64)trap_user_handler;
    p->tf->kernel_hartid = r_tp(); // hartid 用于 CPU ID

    unsigned long x = r_sstatus();
    x &= ~SSTATUS_SPP; 
    x |= SSTATUS_SPIE;
    w_sstatus(x);

    // 设置 S Exception Program Counter 为保存的用户 PC
    w_sepc(p->tf->epc);

     // 告诉 trampoline.S 切换到的用户页表
    uint64 satp = MAKE_SATP(p->pgtbl);

    // 跳转到 trampoline.S，切换到用户页表，恢复用户寄存器，并通过 sret 切换到用户模式
    uint64 fn = TRAMPOLINE + ((uint64)user_return - (uint64)trampoline);
    ((void (*)(uint64, uint64))fn)(TRAPFRAME, satp);
}