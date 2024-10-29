/* memory leyout */
#ifndef __MEMLAYOUT_H__
#define __MEMLAYOUT_H__

// 内核基地址
#define KERNEL_BASE 0x80000000ul

// UART 相关
#define UART_BASE  0x10000000ul
#define UART_IRQ   10

// UART
#define UART_BASE  0x10000000ul
#define UART_IRQ   10
#define UART_REGION_SIZE PGSIZE

// platform-level interrupt controller(PLIC)
#define PLIC_BASE 0x0c000000ul
#define PLIC_PRIORITY(id) (PLIC_BASE + (id) * 4)
#define PLIC_PENDING (PLIC_BASE + 0x1000)
#define PLIC_MENABLE(hart) (PLIC_BASE + 0x2000 + (hart)*0x100)
#define PLIC_SENABLE(hart) (PLIC_BASE + 0x2080 + (hart)*0x100)
#define PLIC_MPRIORITY(hart) (PLIC_BASE + 0x200000 + (hart)*0x2000)
#define PLIC_SPRIORITY(hart) (PLIC_BASE + 0x201000 + (hart)*0x2000)
#define PLIC_MCLAIM(hart) (PLIC_BASE + 0x200004 + (hart)*0x2000)
#define PLIC_SCLAIM(hart) (PLIC_BASE + 0x201004 + (hart)*0x2000)
#define PLIC_REGION_SIZE 0x400000

// core local interruptor(CLINT)
#define CLINT_BASE 0x2000000ul
#define CLINT_MSIP(hartid) (CLINT_BASE + 4 * (hartid))
#define CLINT_MTIMECMP(hartid) (CLINT_BASE + 0x4000 + 8 * (hartid))
#define CLINT_MTIME (CLINT_BASE + 0xBFF8)
#define CLINT_REGION_SIZE 0x10000

// 定义一个相当大的VA, 规定所有VA不得大于它
#define VA_MAX (1ul << 38)

// 定义用户态和内核态切换用到的代码所在的虚拟地址(用户和内核页表都使用)
#define TRAMPOLINE (VA_MAX - PGSIZE)

// 定义用户态和内核态切换用到的数据所在的虚拟地址(仅用户页表使用)
#define TRAPFRAME (TRAMPOLINE - PGSIZE)

// 定义各个进程的内核栈的虚拟地址(间隔分布)
#define KSTACK(id) (TRAPFRAME - ((id) + 1) * 2 * PGSIZE)

#endif