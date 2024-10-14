#include "riscv.h"
#include "lib/print.h"
#include "dev/uart.h"
#include "proc/cpu.h"

volatile static int started = 0;

extern int main();

int main()
{
    // TODO:进行系统的部分初始化
    // 这里先打印一个字符，代表进入了main函数
    if (mycpuid() == 0)
    {
        print_init();
    }
    printf("hart %d starting\n", cpuid());
    while (1)
        ;
}
