# Uno-OS

**所属学校：华东师范大学**

**比赛方向：小型内核实现**

**队伍编号：14620**

**队伍名称：Uno-OS**

**队伍成员：季子墨、李彤**

**指导老师：石亮**

## 简介
UNO-OS是一款RISCV平台的，宏内核操作系统，该内核部分参考了Linux和xv6的设计逻辑，同时也加入了一系列新的设计理念。

详细架构如下
```
./kernel/
├── boot
│   ├── entry.S
│   ├── main.c
│   ├── Makefile
│   └── start.c
├── dev
│   ├── console.c
│   ├── Makefile
│   ├── plic.c
│   ├── timer.c
│   ├── uart.c
│   └── virtio.c
├── fs
│   ├── bitmap.c
│   ├── buf.c
│   ├── dir.c
│   ├── file.c
│   ├── fs.c
│   ├── inode.c
│   └── Makefile
├── kernel.ld
├── lib
│   ├── Makefile
│   ├── print.c
│   ├── sleeplock.c
│   ├── spinlock.c
│   └── str.c
├── Makefile
├── mem
│   ├── kvm.c
│   ├── Makefile
│   ├── mmap.c
│   ├── pmem.c
│   └── uvm.c
├── proc
│   ├── cpu.c
│   ├── exec.c
│   ├── Makefile
│   ├── proc.c
│   └── swtch.S
├── syscall
│   ├── Makefile
│   ├── syscall.c
│   ├── sysfile.c
│   └── sysproc.c
└── trap
    ├── Makefile
    ├── trampoline.S
    ├── trap_kernel.c
    ├── trap.S
    └── trap_user.c 
```

## 完成度

目前已经完成了boot模块，中断处理模块，内存管理模块，进程调度模块，文件系统模块这些操作系统所必备的模块和功能，此外，我们实现了数十条常用的系统调用。

在实现上述模块的同时，为了保证系统的稳定性，我们还进行了大量的测试，包括内核态的测试和用户态的测试。

## 工作量

截至12月24日，该系统总共经历了100余次修改（以commit次数计算）。可以参考我们的[commit记录](https://gitlab.eduxiji.net/T202410269994240/project2608132-270520/-/commits/master)

<!--此处应有表格-->

## 创新点

- 实现了 COW 策略
- 实现了类似 UNIX 下的多级索引文件结构
- 实现了高响应比优先（HRRN）算法
- 优化磁盘缓冲区算法，保证即使在最坏情况下也只需遍历一次链表

## 预计实现的新功能

- 实现UNIX下的管道机制
- 实现懒分配策略
- 进行更加完备的测试
- 实现更多系统调用

上述内容详见[UNO-OS内核设计手册](./UNO-OS内核设计手册)
