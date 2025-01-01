#include "syscall/syscall.h"
#include "lib/print.h"

uint64 sys_curhide()
{
    printf("\033[?25l");
    return 0;
}

uint64 sys_curshow()
{
    printf("\033[?25h");
    return 0;
}

uint64 sys_backcolor()
{
    uint32 red=255,green=0,blue=255;
    printf("\033[48;2;%d;%d;%dm",red,green,blue);
    return 0;
}

uint64 sys_forecolor()
{
    uint32 red=255,green=255,blue=0;
    arg_uint32(0,&red);
    arg_uint32(1,&green);
    arg_uint32(2,&blue);
    printf("\033[38;2;%d;%d;%dm",red,green,blue);
    return 0;
}

uint64 sys_clear()
{
    printf("\033[2J");
    printf("\033[H");// 光标需要回到左上角
    return 0;
}

uint64 sys_conreset()
{
    printf("\033[0m");
    return 0;
}
