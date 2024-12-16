#include "dev/uart.h"
#include "dev/console.h"
#include "proc/cpu.h"
#include "mem/vmem.h"
#include "lib/lock.h"
#include "lib/str.h"
#include "fs/file.h"

#define BACKSPACE 0x100      // 回退
#define CONTROL(x) ((x)-'@') // Control-x

// 外部的设备数组
extern dev_t devlist[N_DEV];

// 控制台
static console_t cons;

void console_init()
{
    spinlock_init(&cons.lk, "console");
    cons.read_idx = 0;
    cons.write_idx = 0;
    cons.edit_idx = 0;

    devlist[DEV_CONSOLE].read = console_read;
    devlist[DEV_CONSOLE].write = console_write;
}

static void console_putchar(int c)
{
    if(c == BACKSPACE) {
        uart_putc_sync('\b');
        uart_putc_sync(' ');
        uart_putc_sync('\b');
    } else {
        uart_putc_sync(c);
    }
}

// 通过UART读取一段字符到 [dst, dst + len)
// 返回读取的长度
uint32 console_read(uint32 len, uint64 dst, bool user)
{
    uint32 read_len = 0;
    int c;
    char cbuf;
    proc_t*  p = myproc(); 
    
    spinlock_acquire(&cons.lk);
    while (read_len < len)
    {
        // 缓冲区为空则休眠
        while (cons.read_idx == cons.write_idx)
            proc_sleep(&cons.read_idx, &cons.lk);

        // 从缓冲区拿出一个字符   
        c = cons.buf[cons.read_idx++ % CONSOLE_INPUT_BUF];
        
        // 遇到EOF则退出
        if(c == CONTROL('D')) {
            if(read_len > 0)
                cons.read_idx--;
            break;
        }

        // 字符的转移
        cbuf = c;
        if(user)
            uvm_copyout(p->pgtbl, dst, (uint64)&cbuf, 1);
        else
            memmove((void*)dst, &cbuf, 1);
        
        // 迭代
        dst++;
        read_len++;

        // 遇到换行符则退出
        if(c == '\n') break;
    }
    spinlock_release(&cons.lk);

    return read_len;
}

// 通过UART输出 [src, src + len) 这段区域的字符
uint32 console_write(uint32 len, uint64 src, bool user)
{
    uint32 cutlen = 0;
    char tmp[CONSOLE_OUTPUT_BUF];
    proc_t* p = myproc();

    spinlock_acquire(&cons.lk);
    while(len > 0) {
        cutlen = MIN(CONSOLE_OUTPUT_BUF, len);
        if(user)
            uvm_copyin(p->pgtbl, (uint64)tmp, src, cutlen);
        else
            memmove(tmp, (void*)src, cutlen);
        
        for(uint32 i = 0; i < cutlen; i++)
            console_putchar(tmp[i]);
        len -= cutlen;
    }
    spinlock_release(&cons.lk);
    return len;
}

// 控制台中断处理
void console_intr(int c)
{
    spinlock_acquire(&cons.lk);

    switch (c)
    {
        // 处理BACKSPACE
        case CONTROL('H'):
        case '\x7f':
            if(cons.edit_idx != cons.write_idx) {
                cons.edit_idx--;
                // 核心操作
                console_putchar(BACKSPACE);
            }
            break;
        
        // 处理其他字符
        default:
            if(c != 0 && cons.edit_idx - cons.read_idx < CONSOLE_INPUT_BUF)
            {
                if(c == '\r') c = '\n';

                // 核心操作: 输出
                console_putchar(c);

                // 核心操作: 写入缓冲区
                cons.buf[cons.edit_idx++ % CONSOLE_INPUT_BUF] = c;

                // 三种情况唤醒读者: 读到换行符 / 读到文件末尾 / 读缓冲区满
                if(c == '\n' || c == CONTROL('D') || cons.edit_idx == cons.read_idx + CONSOLE_INPUT_BUF)
                {
                    cons.write_idx = cons.edit_idx;
                    proc_wakeup(&cons.read_idx);
                }
            }
            break;
    }

    spinlock_release(&cons.lk);
}