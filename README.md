# Uno-OS

## 简介
Uno-OS是一个基于MIT的xv6实验开发的，RISCV架构的简化版UNIX操作系统。

## 睡眠锁
书接上回，我们提到，要让一个进程进入睡眠状态，需调用`sleep`函数，并提供一个`sleep_space`和对应这个`sleep_space`的锁。那么我们很自然地想到，是否可以使用一个数据结构同时管理这两个参数，这个新的数据结构就是我们的睡眠锁，它是专门用于进程的睡眠和唤醒的。与之相对应地，在此前我们曾使用系统时钟来实现进程睡眠一定时间的操作，而系统时钟显然不是专门用于进程的睡眠和唤醒的。

睡眠锁的数据结构如下：

``` c
typedef struct sleeplock {
    spinlock_t lk;
    int locked;
    char* name;
    int pid;
} sleeplock_t;
```

可以看出，`sleeplock`是基于先前的`spinlock`实现的，再来看一下获取和释放`sleeplock`的操作：

``` c
void sleeplock_acquire(sleeplock_t* lk)
{
    spinlock_acquire(&(lk->lk));
    while(lk->locked){
        proc_sleep(lk,&(lk->lk));
    }
    lk->locked=1;
    lk->pid=myproc()->pid;
    spinlock_release(&(lk->lk));
}

void sleeplock_release(sleeplock_t* lk)
{
    spinlock_acquire(&(lk->lk));
    lk->locked=0;
    lk->pid=SLEEPLOCK_INVALID_PID;
    proc_wakeup(lk);
    spinlock_release(&(lk->lk));
}
```

`sleeplock`所持有的自旋锁用于保护`sleeplock`本身（主要是他的`locked`和`pid`字段），保证唤醒信号不会丢失，而由于进入`proc_sleep`后，`sleep_space`对应的锁将被释放，因此同时有多个进程想通过同一个`sleeplock`进入睡眠时，不会被阻塞。

进程的唤醒由`sleeplock_release`将`sleeplock`的`locked`字段设置为0实现。而由于自旋锁的保护，只有一个进程会响应`locked`字段置零的操作，而这个进程响应完毕后又会将`locked`置为1，这样保证了一个`sleeplock_release`操作不会同时唤醒多个进程。

`sleeplock`这一数据结构保证了操作系统的并行性，这对于进程进行磁盘IO时进入睡眠态（或者更广义地，阻塞态）的控制是十分重要的。

## 磁盘中断的注册与处理

### 磁盘中断的注册

磁盘的IO操作是通过PLIC实现的，这需要我们在初始化时对PLIC寄存器进行写入，使其硬盘IO进入使能态。这主要涉及到[`kernel/plic/plic.c`](./kernel/plic/plic.c#8)中的操作。

### 磁盘中断的处理

由于磁盘的IO操作通过PLIC实现，因此，磁盘中断是以外部中断的形式发生的，外部中断的处理主要由`kernel/trap/trap_kernel.c`中的`external_interrupt_handler`函数完成。

我们需要判断外部中断的`irq`号以区分串口IO和磁盘IO操作。

``` c
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
```

`virtio_disk_intr`函数在[`kernel/dev/virtio.c`](kernel/dev/virtio.c#269)中实现。这一函数的许多细节与硬件底层相关，在此不作赘述。

## 磁盘读取缓冲区

各个层级之间的存储设备在访问速度上存在着指数级别的差距（见下图）![memory-hierarchy](./images/memory-hierarchy.png)

这样的差距在内存和磁盘之间体现地尤为明显，因此，尽量减少磁盘的IO操作次数可以很大程度上提高程序的性能。

类似在CPU寄存器和内存之间设计cache的操作，我们可以在内存中开辟一块空间用于缓存从磁盘中读取的数据，当需要访问磁盘时，首先检查目标数据是否存在于缓存中，若不存在则将数据从磁盘中读取到缓冲区中，再从缓冲区中读取数据。根据局部性原理，这样的操作显然可以提高系统性能。

我们在硬件层之上设计了一个缓存层，这一层主要在[`kernel/fs/buf.c`](./kernel/fs/buf.c)中实现。这样所有的磁盘IO操作都需要经过缓存层执行。

缓存的设计我们采取`LRU`和`Lazy write`策略，更进一步地提升其性能。Uno-OS启动时首先静态分配所有的缓存块（个数由[`N_BLOCK_BUF`](./kernel/fs/buf.c#7)决定）。然后，将他们组织成双向循环列表，另外分配一个用作指示的buf_node作为[整个链表的头](./kernel/fs/buf.c#19)
