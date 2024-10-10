#include "riscv.h"

__attribute__ ((aligned (16))) uint8 CPU_stack[4096 * NCPU];

extern int main();

// 负责M模式到S模式的切换并跳转进入main函数
void start()
{
    // 从M模式变为S模式
    uint64 mstatus=r_mstatus();

    // 设置MPP
    // MPP中的特权级字段会在执行mret时恢复
    mstatus&=~MSTATUS_MPP_MASK;
    mstatus|=MSTATUS_MPP_S; // mret时进入S模式
    w_mstatus(mstatus);
    
    // mepc保存mret时的返回地址
    w_mepc((uint64)main);

    // 设置satp，暂时禁用页表
    w_satp(0);

    // 异常和中断全部交由S模式处理
    w_medeleg(0xffff);
    w_mideleg(0xffff);
    
    // S模式处理某一中断i的条件：
    // i) 当前特权级模式比S低或当前特权级模式为S且设置了sstatus里面的SIE位。
    // ii) sip和sie中位i被置位。
    // 这里允许所有的异常和中断被S模式处理
    w_sie(r_sie()|SIE_SEIE|SIE_SSIE|SIE_STIE);

    uint64 id = r_mhartid();
    w_tp(id);

    asm volatile ("mret");

}