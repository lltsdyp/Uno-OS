#include "riscv.h"
#include "lib/print.h"
#include "dev/uart.h"
#include "proc/cpu.h"

volatile static int started = 0;

extern int main();

int main()
{
    int val1 = 1;
    int val2 = 2;
    int val3 = 3;
    // TODO:进行系统的部分初始化
    // 这里先打印一个字符，代表进入了main函数
    if (mycpuid() == 0)
    {
        print_init();
        // 调试信息
        // printf("hart %d starting\n", mycpuid());
        // panic("%d %d %d\n", val1, val2, val3);
    }
    printf("hart %d starting\n", mycpuid());
    while (1)
        ;
}
