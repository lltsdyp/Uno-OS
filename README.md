# Uno-OS

## 简介
Uno-OS是一个基于MIT的xv6实验开发的，RISCV架构的简化版UNIX操作系统。

## lab-1

### 启动过程
RISCV架构中，系统启动需要经过一个较为固定的过程：从0x80000000开始，这个地方被称为引导扇区，加载`entry.S`中的内容。这个程序需要将内核栈初始化，然后进入`start`函数中执行必要的初始化
``` asm
.section .text
_entry:
        # CPU_stack 定义于start.c中
        # sp = CPU_stack + ((hartid + 1) * 4096)
        # 将sp置于当前CPU的内核栈的栈顶
        la sp, CPU_stack
        li a0, 4096
        csrr a1, mhartid
        addi a1, a1, 1
        mul a0, a0, a1
        add sp, sp, a0
        # 跳转到start
        call start
spin:
        j spin
```
`start`函数中，我已经将所有语句加上了详细的注释，这个函数的大致功能是，在M模式下，设置即将进入S模式后中断的处理方法，页表使用情况等，同时初始化cpu序号，最后跳转到main函数并进入S模式，此后内核的绝大部分操作都是在S模式下执行的。
``` c
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
```

进入`main`函数后，我们初始化uart串口，并输出cpu启动的信息
``` c
#include "riscv.h"
#include "lib/print.h"
#include "dev/uart.h"
#include "proc/cpu.h"

volatile static int started = 0;

extern int main();

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
```

### 自旋锁的实现
要实现uart串口的顺序输出，我们必须要使用自旋锁，使得同一时间只能有一个`printf`在执行
``` c
void vprintf(const char *fmt, va_list ap)  
{  
    int c;  
    char *s;  
    spinlock_acquire(&print_lk); // 加锁以确保线程安全  


    for (int i = 0; (c = fmt[i] & 0xff) != 0; ++i)  
    {  
        if (c != '%')  
        {  
            consputc(c); // 直接输出字符  
            continue;  
        }  

        c = fmt[++i] & 0xff; // 获取下一个字符  
        if (c == 0)  
            break; // 如果下一个字符是结尾，退出  

        switch (c)  
        {  
        case 'd':  
            printint(va_arg(ap, int), 10, 1); // 输出十进制整数  
            break;  
        case 'x':  
            printint(va_arg(ap, int), 16, 1); // 输出十六进制整数  
            break;  
        case 'p':  
            printptr(va_arg(ap, uint64)); // 输出指针  
            break;  
        case 's':  
            if ((s = va_arg(ap, char *)) == 0)  
                s = "(null)"; // 处理空字符串  
            for (; *s; ++s)  
                consputc(*s); // 输出字符串中的每个字符  
            break;  
        case '%':  
            consputc('%'); // 输出百分号  
            break;  
        default:  
            // 输出未知的格式符  
            consputc('%');  
            consputc(c);  
            break;  
        }  
    }  

    spinlock_release(&print_lk); // 解锁  
}


// Print to the console. only understands %d, %x, %p, %s.
// 只需要实现四种输出
void printf(const char *fmt, ...)
{
    va_list ap;
    if (fmt == 0)  
        panic("null fmt"); // 检查格式字符串是否为空  
    va_start(ap, fmt);

    vprintf(fmt,ap);

    va_end(ap);
}
```
锁的实现在`spinlock.h`中：为了实现锁的取得和释放，我们需要关闭中断，而关闭中断需要实现信号量的操作，否则容易引起死锁问题，`noff`就是一个关中断的信号量：
``` c
// 带层数叠加的关中断
void push_off(void)
{
    mycpu()->noff++;
    int interrupt_status=intr_get();

    // 层数大于等于1，说明需要关闭中断
    if(mycpu()->noff>=1)
    {
        intr_off();
        if(mycpu()->noff==1)
        {
            mycpu()->origin=interrupt_status;
        }
    }
}

// 带层数叠加的开中断
void pop_off(void)
{
    int interrupt_status=intr_get();

    assert(interrupt_status==0, "spinlock_pop_off: interruptible");
    assert(mycpu()->noff>=0, "spinlock_pop_off: no interrupts were disabled!");
    mycpu()->noff--;
    if(mycpu()->noff==0)
    {
        intr_on();
    }
}
```
每当进行锁的获取时，我们将中断关闭，避免中断引发死锁。同样，当释放锁时我们也会开中断。
