// 标准输出和报错机制
#include <stdarg.h>
#include "lib/print.h"
#include "lib/lock.h"
#include "dev/uart.h"

#define BACKSPACE 0x100
volatile int panicked = 0;

static spinlock_t print_lk;

static char digits[] = "0123456789abcdef";

void print_init(void)
{
    uart_init();
    spinlock_init(&print_lk, "pr");
}

// 向控制台输出一个字符
void consputc(int c)
{
    if (c == BACKSPACE)
    {
        // if the user typed backspace, overwrite with a space.
        uart_putc_sync('\b');
        uart_putc_sync(' ');
        uart_putc_sync('\b');
    }
    else
    {
        uart_putc_sync(c);
    }
}

// 可以输出%d和%x两种类型变量
static void printint(int xx, int base, int sign)
{
    char buf[16]; // 用于存储数值转换后的数
    int len;      // 记录转化后数组的长度
    uint32 x;     // 储存xx的绝对值

    // 如果xx是有符号整数且为正数，则会将sign更新成0
    // 因此，只有当xx是有符号整数且为负数的时候sign的值才会是非零
    // 可以用于后面对xx正负性的判断
    if (sign && (xx < 0))
        x = -xx;
    else
        x = xx;

    len = 0;
    do
    {
        // 将数值x转化成base进制，并将数据存储在buf中
        buf[len++] = digits[x % base];
    } while ((x /= base) != 0);

    if (xx < 0)
        consputc('-');

    while (len > 0)
        consputc(buf[--len]);
}

// 定义一个静态函数，用于打印64位无符号整数的十六进制表示
static void printptr(uint64 x)
{
    consputc('0');
    consputc('x');
    // 循环遍历64位整数的每个字节，将其转换为十六进制字符并发送
    for (int i = 0; i < (sizeof(uint64) * 2); i++, x <<= 4)
        consputc(digits[x >> (sizeof(uint64) * 8 - 4)]);
}

// Print formatted output with a va_list argument
void vprintf(const char *fmt, va_list ap)
{
    int c;
    char *s;
    // spinlock_acquire(&print_lk); // 加锁以确保线程安全

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

    // spinlock_release(&print_lk); // 解锁
}

// Print to the console. only understands %d, %x, %p, %s.
// 只需要实现四种输出
void printf(const char *fmt, ...)
{
    va_list ap;  

    if (fmt == NULL)  
        panic("null fmt");  
    va_start(ap, fmt);  
    spinlock_acquire(&print_lk); // 加锁在printf中  

    vprintf(fmt, ap); // 调用vprintf，不再加锁  

    va_end(ap);  
    spinlock_release(&print_lk); // 解锁 
}

void panic(const char *fmt, ...)
{
    va_list ap;

    // 初始化可变参数列表 `ap`
    va_start(ap, fmt);
    printf("panic: ");

    vprintf(fmt, ap);
    printf("\n");
    va_end(ap);

    // 设置 panicked 标志位，冻结其他 CPU 的 UART 输出
    panicked = 1;

    // 进入无限循环，使程序在此处停止运行
    while (1)
        ;
}

void assert(int condition, const char *warning, ...)
{
    if (!condition)
    {
        va_list ap;
        va_start(ap, warning);
        panic(warning, ap); // 调用时只传递 warning，处理 ap 在 panic 内
        va_end(ap);
    }
}