#include "lib/print.h"
#include "dev/timer.h"
#include "dev/uart.h"
#include "dev/plic.h"
#include "trap/trap.h"
#include "proc/cpu.h"
#include "memlayout.h"
#include "riscv.h"

#include "trap/intrno.h"

void virtio_disk_intr();

// 中断信息
char* interrupt_info[16] = {
    "U-mode software interrupt",      // 0
    "S-mode software interrupt",      // 1
    "reserved-1",                     // 2
    "M-mode software interrupt",      // 3
    "U-mode timer interrupt",         // 4
    "S-mode timer interrupt",         // 5
    "reserved-2",                     // 6
    "M-mode timer interrupt",         // 7
    "U-mode external interrupt",      // 8
    "S-mode external interrupt",      // 9
    "reserved-3",                     // 10
    "M-mode external interrupt",      // 11
    "reserved-4",                     // 12
    "reserved-5",                     // 13
    "reserved-6",                     // 14
    "reserved-7",                     // 15
};

// 异常信息
char* exception_info[16] = {
    "Instruction address misaligned", // 0
    "Instruction access fault",       // 1
    "Illegal instruction",            // 2
    "Breakpoint",                     // 3
    "Load address misaligned",        // 4
    "Load access fault",              // 5
    "Store/AMO address misaligned",   // 6
    "Store/AMO access fault",         // 7
    "Environment call from U-mode",   // 8
    "Environment call from S-mode",   // 9
    "reserved-1",                     // 10
    "Environment call from M-mode",   // 11
    "Instruction page fault",         // 12
    "Load page fault",                // 13
    "reserved-2",                     // 14
    "Store/AMO page fault",           // 15
};

// in trap.S
// 内核中断处理流程
extern void kernel_vector();

// 初始化trap中全局共享的东西
void trap_kernel_init()
{
    timer_create();
}

// 各个核心trap初始化
// 即设置stvec寄存器，发生中断跳转到kernel_vector函数处理
void trap_kernel_inithart()
{
    w_stvec((uint64)kernel_vector);

    intr_on();
    mycpu()->noff = 0; //强制打开
}

// 外设中断处理 (基于PLIC)
void external_interrupt_handler()  
{  
    int irq = plic_claim(); // 获取中断号
    
    if (!irq) {
        return ;      // 中断号为0，直接忽略；
    } 
    else if (irq == UART_IRQ) 
    {  
        uart_intr(); // 调用 UART 中断处理程序 
    } else if(irq==VIRTIO_IRQ)
    {
        virtio_disk_intr();
    }
    else{
        printf("Unexpected interrupt irq = %d\n", irq);
    }

    plic_complete(irq); // 确认中断处理完成
}

// 时钟中断处理 (基于CLINT)
void timer_interrupt_handler()
{
    // 清除中断标志位
    w_sip(r_sip() & ~2);
    // 避免重复更新时钟
    if(mycpuid()==0)
        timer_update();
    // 实现公平调度，避免长期独占    
    if(myproc() != NULL && myproc()->state == RUNNING)
        proc_yield();
}

// 在kernel_vector()里面调用
// 内核态trap处理的核心逻辑
void trap_kernel_handler()
{
    uint64 sepc = r_sepc();          // 记录了发生异常时的pc值
    uint64 sstatus = r_sstatus();    // 与特权模式和中断相关的状态信息
    uint64 scause = r_scause();      // 引发trap的原因
    uint64 stval = r_stval();        // 发生trap时保存的附加信息(不同trap不一样)

    // 确认trap来自S-mode且此时trap处于关闭状态
    assert(sstatus & SSTATUS_SPP, "trap_kernel_handler: not from s-mode");
    assert(intr_get() == 0, "trap_kernel_handler: interreput enabled");

    int trap_id = scause & 0xf; 

    // 暂未实现对异常的处理
    assert(IS_INTR(scause),"Unhandled exception,\n\tsepc:%p,scause:%p,sstatus:%p,stval:%p,trap_id:%d\n\tdescription:%s"
                ,sepc,scause,sstatus,stval,trap_id,exception_info[trap_id]);

    // 中断异常处理核心逻辑

    // M模式下的时钟中断会触发S模式下的软件中断，因此判断trap_id是否为SMODE_SOFTWARE_INTERRUPT即可
    // 新增中断处理表项时需在trap/intrno.h中添加常量定义，不要使用magic number.
    switch(trap_id)
    {
        case SMODE_SOFTWARE_INTERRUPT:
            timer_interrupt_handler();
            break;
        case SMODE_EXTERNAL_INTERRUPT:
            external_interrupt_handler();
            break;
        default:
            panic("Unknown trap id %x,\n\tdescription:%s",trap_id,interrupt_info[trap_id]);
    }

    // 恢复前述状态
    w_sepc(sepc);
    w_sstatus(sstatus);
}
// TODO:增加磁盘中断
