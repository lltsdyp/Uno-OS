// 标准输出和报错机制
#include <stdarg.h>
#include "lib/print.h"
#include "lib/lock.h"
#include "dev/uart.h"

volatile int panicked = 0;

static spinlock_t print_lk;

static char digits[] = "0123456789abcdef";

// 借鉴xv6
void print_init(void)
{
    initlock(&pr.lock, "pr");
    pr.locking = 1;
}

// 可以输出%d和%x两种类型变量
static void printint(int xx, int base, int sign)
{
    char buf[16];   // 用于存储数值转换后的数
    int len;        // 记录转化后数组的长度
    uint x;         // 得到xx的绝对值

    // 如果xx是有符号整数且为正数，则会将sign更新成0
    // 因此，只有当xx是有符号整数且为负数的时候sign的值才会是非零
    // 可以用于后面对xx正负性的判断
    if(sign && (sign = xx < 0))
        x = -xx;
    else
        x = xx;

    len = 0;
    do {
        // 将数值x转化成base进制，并将数据存储在buf中
        buf[len++] = digits[x % base];
        x /= base;
    } while (x != 0);

    if(sign)        // 对应前面sign更新用于判断xx正负性
        buf[len++] = '-'; 
    
    while(--len > 0)
        uart_putc_sync(buf[len])
}

// 定义一个静态函数，用于打印64位无符号整数的十六进制表示
static void printptr(uint64 x)
{
    int i; // 定义一个整型变量i，用于循环计数
    uart_putc_sync('0'); // 通过UART同步发送字符'0'
    uart_putc_sync('x'); // 通过UART同步发送字符'x'
    // 循环遍历64位整数的每个字节，将其转换为十六进制字符并发送
    for (i = 0; i < (sizeof(uint64) * 2); i++, x << 4)
        uart_putc_sync(digits[x >> (sizeof(uint64) * 8 - 4) & 0xf]);
}

// Print to the console. only understands %d, %x, %p, %s.
// 只需要实现四种输出
void printf(const char *fmt, ...)
{
    va_list ap;
    int i, c;
    char *s;
    if (fmt == 0)
        panic("null fmt");

    // 从fmt开始遍历可变参数列表ap
    va_start(ap, fmt);
    for (i = 0; (c = fmt[i] & 0xff) != 0; ++i)
    {
        // 当c！='%'时，说明非变量，直接输出内容就可以
        if (c != '%'){
            uart_putc_sync(c);
            continue;
        }

        c = fmt[++i] & 0xff;
        if (c == 0)
            break;

        switch(c){
            // va_arg获得下一个参数
            case 'd':
                printint(va_arg(ap, int), 10, 1);
                break;
            case 'x':
                printint(va_arg(ap, int), 16, 1);
                break;
            case 'p':
                printptr(va_arg(ap, uint64));
                break;
            case 's':
                if ((s = va_arg(ap, char*)) == 0)   // 字符串为空
                    s = "(null)";
                for (; *s; ++s)
                    uart_putc_sync(*s);
                break;
            // 转义输出'%'
            case '%':
                uart_putc_sync('%')
                break;
            // 其他情况直接输出
            default:
                uart_putc_sync('%');
                uart_putc_sync(c);
                break;
        }
    }
}

// 未验证正确性
void panic(const char *fmt, ...)
{
    va_list ap;

    // 初始化可变参数列表 `ap`
    va_start(ap, fmt);
    printf("panic: ");

    //  `printf(fmt, ap)` 会使用 `fmt` 格式化字符串和 `ap` 中的可变参数
    printf(fmt, ap);
    va_end(ap);
    printf("\n");
    
    // 设置 panicked 标志位，冻结其他 CPU 的 UART 输出
    panicked = 1;

    // 进入无限循环，使程序在此处停止运行
    while (1)
        ;
}

// 未验证正确性
void assert(bool condition, const char* warning, ...)
{
    if (!condition)
    {
        va_list ap;
        va_start(ap, warning);
        panic(warning, ap);
        va_end(ap);
    }
}