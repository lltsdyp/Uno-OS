#include "lib/print.h"
#include "trap/trap.h"
#include "proc/cpu.h"
#include "mem/vmem.h"
#include "memlayout.h"
#include "riscv.h"

// in trampoline.S
extern char trampoline[];      // 内核和用户切换的代码
extern char user_vector[];     // 用户触发trap进入内核
extern char user_return[];     // trap处理完毕返回用户

// in trap.S
extern char kernel_vector[];   // 内核态trap处理流程

// in trap_kernel.c
extern char* interrupt_info[16]; // 中断错误信息
extern char* exception_info[16]; // 异常错误信息

// 在user_vector()里面调用
// 用户态trap处理的核心逻辑
void trap_user_handler()
{
    uint64 sepc = r_sepc();          // 记录了发生异常时的pc值
    uint64 sstatus = r_sstatus();    // 与特权模式和中断相关的状态信息
    uint64 scause = r_scause();      // 引发trap的原因
    uint64 stval = r_stval();        // 发生trap时保存的附加信息(不同trap不一样)
    proc_t* p = myproc();

    // 确认trap来自U-mode
    assert((sstatus & SSTATUS_SPP) == 0, "trap_user_handler: not from u-mode");

}

// 调用user_return()
// 内核态返回用户态
void trap_user_return()
{

}