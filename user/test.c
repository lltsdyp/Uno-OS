// 来自 test.c
// 测试目的: 检测printf的正确性 + exec的传参能力
#include "userlib.h"

int main(int argc, char* argv[])
{
    printf("\nchild arguments %d: ",argc);
    for(int i = 0; i < argc; i++)
        printf("%s ", argv[i]);
    printf("\n");
    return 0;
}