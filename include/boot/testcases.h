#ifndef __TESTCASES_H__
#define __TESTCASES_H__

#ifndef __INITCODE_C__

#include "riscv.h"
#include "lib/print.h"
#include "dev/uart.h"
#include "proc/cpu.h"
#include "mem/pmem.h"
#include "mem/vmem.h"
#include "lib/str.h"
#include "trap/trap.h"
#include "dev/plic.h"
#include "proc/proc.h"

#else

#include "sys.h"

#endif // initcode.c

volatile static int started = 0;

extern int main();

#ifdef LAB_1_CASE_1

int main()
{
    // int val1 = 1;
    // int val2 = 2;
    // int val3 = 3;
    // TODO:进行系统的部分初始化
    // 这里先打印一个字符，代表进入了main函数
    if (mycpuid() == 0)
    {
        print_init();
        // 调试信息
        // printf("hart %d starting\n", mycpuid());
        // panic("%d %d %d\n", val1, val2, val3);
        printf("%p\n", 0x3fffffe000);
        started = 1;
    }
    else
    {
        // 等待cpu0完成所有启动所需的初始化工作
        while (!started)
            ;
    }
    printf("hart %d starting\n", mycpuid());
    while (1)
        ;
}

#elif defined LAB_2_CASE_1

volatile static int over_1 = 0, over_2 = 0;

static int *mem[1024];

int main()
{
    int cpuid = r_tp();

    if (cpuid == 0)
    {

        print_init();
        pmem_init();

        printf("cpu %d is booting!\n", cpuid);
        __sync_synchronize();
        started = 1;

        for (int i = 0; i < 512; i++)
        {
            mem[i] = pmem_alloc(true);
            memset(mem[i], 1, PGSIZE);
            printf("mem = %p, data = %d\n", mem[i], mem[i][0]);
        }
        printf("cpu %d alloc over\n", cpuid);
        over_1 = 1;

        while (over_1 == 0 || over_2 == 0)
            ;

        for (int i = 0; i < 512; i++)
            pmem_free((uint64)mem[i], true);
        printf("cpu %d free over\n", cpuid);
    }
    else
    {

        while (started == 0)
            ;
        __sync_synchronize();
        printf("cpu %d is booting!\n", cpuid);

        for (int i = 512; i < 1024; i++)
        {
            mem[i] = pmem_alloc(true);
            memset(mem[i], 1, PGSIZE);
            printf("mem = %p, data = %d\n", mem[i], mem[i][0]);
        }
        printf("cpu %d alloc over\n", cpuid);
        over_2 = 1;

        while (over_1 == 0 || over_2 == 0)
            ;

        for (int i = 512; i < 1024; i++)
            pmem_free((uint64)mem[i], true);
        printf("cpu %d free over\n", cpuid);
    }
    while (1)
        ;
}

#elif defined LAB_2_CASE_2

int main()
{
    int cpuid = r_tp();

    if (cpuid == 0)
    {

        print_init();
        pmem_init();
        kvm_init();
        kvm_inithart();
        printf("cpu %d is booting!\n", cpuid);

        __sync_synchronize();
        // started = 1;

        pgtbl_t test_pgtbl = pmem_alloc(true);
        uint64 mem[5];
        for (int i = 0; i < 5; i++)
            mem[i] = (uint64)pmem_alloc(false);

        printf("\ntest-1\n\n");
        vm_mappages(test_pgtbl, 0, mem[0], PGSIZE, PTE_R);
        vm_mappages(test_pgtbl, PGSIZE * 10, mem[1], PGSIZE / 2, PTE_R | PTE_W);
        vm_mappages(test_pgtbl, PGSIZE * 512, mem[2], PGSIZE - 1, PTE_R | PTE_X);
        vm_mappages(test_pgtbl, PGSIZE * 512 * 512, mem[2], PGSIZE, PTE_R | PTE_X);
        vm_mappages(test_pgtbl, VA_MAX - PGSIZE, mem[4], PGSIZE, PTE_W);
        vm_print(test_pgtbl);

        printf("\ntest-2\n\n");
        vm_mappages(test_pgtbl, 0, mem[0], PGSIZE, PTE_W);
        vm_unmappages(test_pgtbl, PGSIZE * 10, PGSIZE, true);
        vm_unmappages(test_pgtbl, PGSIZE * 512, PGSIZE, true);
        vm_print(test_pgtbl);
    }
    else
    {

        while (started == 0)
            ;
        __sync_synchronize();
        printf("cpu %d is booting!\n", cpuid);
    }
    while (1)
        ;
}

#elif defined LAB_3_CASE_1

int main()
{
    intr_off();
    if (mycpuid() == 0)
    {
        print_init();
        pmem_init();
        kvm_init();
        kvm_inithart();
        trap_kernel_init();
        trap_kernel_inithart();
        plic_init();
        plic_inithart();
        __sync_synchronize();
        started = 1;
    }
    else
    {
        // 等待cpu0完成所有启动所需的初始化工作
        while (!started)
            ;
        __sync_synchronize();
        kvm_inithart();
        trap_kernel_inithart();
        plic_inithart();
    }
    printf("hart %d starting\n", mycpuid());
    while (1)
        ;
}

#elif defined LAB_4_CASE_1

int main()
{
    int cpuid = r_tp();

    if (cpuid == 0)
    {

        print_init();
        printf("cpu %d is booting!\n", cpuid);

        pmem_init();
        kvm_init();
        kvm_inithart();
        trap_kernel_init();
        trap_kernel_inithart();
        plic_init();
        plic_inithart();

        proc_make_first();

        __sync_synchronize();
        // started = 1;
    }
    else
    {

        while (started == 0)
            ;
        __sync_synchronize();
        printf("cpu %d is booting!\n", cpuid);
        kvm_inithart();
        trap_kernel_inithart();
        plic_inithart();
    }
    printf("Ready\n");
    while (1)
        ;
}

#elif defined LAB_5_CASE_1

int main()
{
    int L[5];
    char *s = "hello, world";
    syscall(SYS_copyout, L);
    syscall(SYS_copyin, L, 5);
    syscall(SYS_copyinstr, s);
    while (1)
        ;
    return 0;
}

#elif defined LAB_5_CASE_2

int main()
{
    long long heap_top = syscall(SYS_brk, 0);

    heap_top = syscall(SYS_brk, heap_top + 4096 * 10);

    heap_top = syscall(SYS_brk, heap_top - 4096 * 5);

    while (1)
        ;
    return 0;
}

#elif defined LAB_5_CASE_3

#error "Special case, please uncomment the code in main.c rather than define macro LAB_5_CASE_3"

#elif defined LAB_5_CASE_4

#include "sys.h"

// 与内核保持一致
#define VA_MAX (1ul << 38)
#define PGSIZE 4096
#define MMAP_END (VA_MAX - 34 * PGSIZE)
#define MMAP_BEGIN (MMAP_END - 8096 * PGSIZE)

int main()
{
    // 建议画图理解这些地址和长度的含义

    // sys_mmap 测试
    syscall(SYS_mmap, MMAP_BEGIN + 4 * PGSIZE, 3 * PGSIZE);
    syscall(SYS_mmap, MMAP_BEGIN + 10 * PGSIZE, 2 * PGSIZE);
    syscall(SYS_mmap, MMAP_BEGIN + 2 * PGSIZE, 2 * PGSIZE);
    syscall(SYS_mmap, MMAP_BEGIN + 12 * PGSIZE, 1 * PGSIZE);
    syscall(SYS_mmap, MMAP_BEGIN + 7 * PGSIZE, 3 * PGSIZE);
    syscall(SYS_mmap, MMAP_BEGIN, 2 * PGSIZE);
    syscall(SYS_mmap, 0, 10 * PGSIZE);
    syscall(SYS_mmap,MMAP_END-3*PGSIZE,3*PGSIZE);

    // sys_munmap 测试
    syscall(SYS_munmap, MMAP_BEGIN + 10 * PGSIZE, 5 * PGSIZE);
    syscall(SYS_munmap, MMAP_BEGIN, 10 * PGSIZE);
    syscall(SYS_munmap, MMAP_BEGIN + 17 * PGSIZE, 2 * PGSIZE);
    syscall(SYS_munmap, MMAP_BEGIN + 15 * PGSIZE, 2 * PGSIZE);
    syscall(SYS_munmap, MMAP_BEGIN + 19 * PGSIZE, 2 * PGSIZE);
    syscall(SYS_munmap, MMAP_BEGIN + 22 * PGSIZE, 1 * PGSIZE);
    syscall(SYS_munmap,MMAP_END-PGSIZE,PGSIZE);
    syscall(SYS_munmap,MMAP_END-3*PGSIZE,PGSIZE);
    syscall(SYS_munmap,MMAP_END-2*PGSIZE,PGSIZE);
    syscall(SYS_munmap, MMAP_BEGIN + 21 * PGSIZE, 1 * PGSIZE);

    while (1)
        ;
    return 0;
}

#elif defined LAB_6_CASE_1

#include "sys.h"

// 与内核保持一致
#define VA_MAX       (1ul << 38)
#define PGSIZE       4096
#define MMAP_END     (VA_MAX - 34 * PGSIZE)
#define MMAP_BEGIN   (MMAP_END - 8096 * PGSIZE) 

char *str1, *str2;

int main()
{
    syscall(SYS_print, "\nuser begin\n");

    // 测试MMAP区域
    str1 = (char*)syscall(SYS_mmap, MMAP_BEGIN, PGSIZE);
    
    // 测试HEAP区域
    long long top = syscall(SYS_brk, 0);
    str2 = (char*)top;
    syscall(SYS_brk, top + PGSIZE);

    str1[0] = 'M';
    str1[1] = 'M';
    str1[2] = 'A';
    str1[3] = 'P';
    str1[4] = '\n';
    str1[5] = '\0';

    str2[0] = 'H';
    str2[1] = 'E';
    str2[2] = 'A';
    str2[3] = 'P';
    str2[4] = '\n';
    str2[5] = '\0';

    int pid = syscall(SYS_fork);

    if(pid == 0) { // 子进程
        for(int i = 0; i < 100000000; i++);
        syscall(SYS_print, "child: hello\n");
        syscall(SYS_print, str1);
        syscall(SYS_print, str2);

        syscall(SYS_exit, 1);
        syscall(SYS_print, "child: never back\n");
    } else {       // 父进程
        int exit_state;
        syscall(SYS_wait, &exit_state);
        if(exit_state == 1)
            syscall(SYS_print, "parent: hello\n");
        else
            syscall(SYS_print, "parent: error\n");
    }

    while(1);
    return 0;
}

#elif defined LAB_6_CASE_2

#include "sys.h"

int main()
{
    syscall(SYS_fork);
    syscall(SYS_fork);

    while(1);
    return 0;
}

#elif defined LAB_6_CASE_3

#include "sys.h"

int main()
{
    int pid=syscall(SYS_fork);

    if(pid==0){
        syscall(SYS_print,"sleep!\n");
        syscall(SYS_sleep,1);
        syscall(SYS_print,"wakeup!\n");
        syscall(SYS_exit,1);
    }else {       // 父进程
        int exit_state;
        syscall(SYS_wait, &exit_state);
        if(exit_state == 1)
            syscall(SYS_print, "parent: hello\n");
        else
            syscall(SYS_print, "parent: error\n");
    }

    while(1);
    return 0;
}

#elif defined LAB_7_CASE_1

#include "sys.h"
#include "type.h"
int main()
{
    uint32 block_num_1 = syscall(SYS_alloc_block);
    uint32 block_num_2 = syscall(SYS_alloc_block);
    uint32 block_num_3 = syscall(SYS_alloc_block);
    syscall(SYS_free_block, block_num_2);
    syscall(SYS_free_block, block_num_1);
    syscall(SYS_free_block, block_num_3);
    
    while(1);
    return 0;
}

#elif defined LAB_7_CASE_2

#include "sys.h"
#include "type.h"

int main()
{
    char buf[128];
    uint64 buf_in_kernel[10];

    // 初始状态:读了sb并释放了buf
    syscall(SYS_print, "\nstate-1:");
    syscall(SYS_show_buf);
    
    // 耗尽所有 buf
    for(int i = 0; i < 6; i++) {
        buf_in_kernel[i] = syscall(SYS_read_block, 100 + i, buf);
        buf[i] = 0xFF;
        syscall(SYS_write_block, buf_in_kernel[i], buf);
    }
    syscall(SYS_print, "\nstate-2:");
    syscall(SYS_show_buf);

    // 测试是否会触发buf_read里的panic,测试完后注释掉(一次性)
    // buf_in_kernel[0] = syscall(SYS_read_block, 0, buf);


    // 释放两个buf-4 和 buf-1，查看链的状态
    syscall(SYS_release_block, buf_in_kernel[3]);
    syscall(SYS_release_block, buf_in_kernel[0]);
    syscall(SYS_print, "\nstate-3:");
    syscall(SYS_show_buf);

    // 申请buf,测试LRU是否生效 + 测试103号block的lazy write
    buf_in_kernel[6] = syscall(SYS_read_block, 106, buf);
    buf_in_kernel[7] = syscall(SYS_read_block, 103, buf);
    syscall(SYS_print, "\nstate-4:");
    syscall(SYS_show_buf);

    // 释放所有buf
    syscall(SYS_release_block, buf_in_kernel[7]);
    syscall(SYS_release_block, buf_in_kernel[6]);
    syscall(SYS_release_block, buf_in_kernel[5]);
    syscall(SYS_release_block, buf_in_kernel[4]);
    syscall(SYS_release_block, buf_in_kernel[2]);
    syscall(SYS_release_block, buf_in_kernel[1]);
    syscall(SYS_print, "\nstate-5:");
    syscall(SYS_show_buf);

    while(1);
    return 0;
}

#elif defined LAB_7_CASE_3

#include "sys.h"
#include "type.h"

int main()
{
    char buf[128];
    uint64 buf_in_kernel[10];

    // 初始状态:读了sb并释放了buf
    syscall(SYS_print, "\nstate-1:");
    syscall(SYS_show_buf);
    
    // buf不够用怎么办？
    for(int i = 0; i < 7; i++) {
        buf_in_kernel[i] = syscall(SYS_read_block, 100 + i, buf);
        buf[i] = 0xFF;
        syscall(SYS_write_block, buf_in_kernel[i], buf);
    }
    syscall(SYS_print, "\nstate-2:");
    syscall(SYS_show_buf);

    // 测试是否会触发buf_read里的panic,测试完后注释掉(一次性)
    // buf_in_kernel[0] = syscall(SYS_read_block, 0, buf);


    // 释放两个buf-4 和 buf-1，查看链的状态
    syscall(SYS_release_block, buf_in_kernel[3]);
    syscall(SYS_release_block, buf_in_kernel[0]);
    syscall(SYS_print, "\nstate-3:");
    syscall(SYS_show_buf);

    // 申请buf,测试LRU是否生效 + 测试103号block的lazy write
    buf_in_kernel[6] = syscall(SYS_read_block, 106, buf);
    buf_in_kernel[7] = syscall(SYS_read_block, 103, buf);
    syscall(SYS_print, "\nstate-4:");
    syscall(SYS_show_buf);

    // 释放所有buf
    syscall(SYS_release_block, buf_in_kernel[7]);
    syscall(SYS_release_block, buf_in_kernel[6]);
    syscall(SYS_release_block, buf_in_kernel[5]);
    syscall(SYS_release_block, buf_in_kernel[4]);
    syscall(SYS_release_block, buf_in_kernel[2]);
    syscall(SYS_release_block, buf_in_kernel[1]);
    syscall(SYS_print, "\nstate-5:");
    syscall(SYS_show_buf);

    while(1);
    return 0;
}

#elif defined LAB_8_CASE_1

#include "sys.h"
#include "type.h"

int main()
{
    syscall(SYS_print,"Jump to initcode.\n");
    
    while(1);
    return 0;
}

#elif defined LAB_9_CASE_1


#include "sys.h"

int main()
{
    // char path[] = "./test";
    // char* argv[] = {"hello", "world", 0};
    syscall(SYS_write, 0, 5, "Init\n");


    // int pid = syscall(SYS_fork);
    // if(pid < 0) { // 失败
    //     syscall(SYS_write, 0, 20, "initcode: fork fail\n");
    // } else if(pid == 0) { // 子进程
    //     syscall(SYS_write, 0, 22, "\n-----test start-----\n");
    //     syscall(SYS_exec, path, argv);
    // } else { // 父进程
    //     int exit_state;
    //     syscall(SYS_wait, &exit_state);
    //     syscall(SYS_write, 0, 21, "\n-----test over-----\n");
    //     while(1);
    // }
    while(1);
    return 0;
}

#elif defined LAB_9_CASE_2

// 来自 test.c
// 测试目的: 检测printf的正确性 + exec的传参能力
#include "userlib.h"

int main(int argc, char* argv[])
{
    printf("\nchild arguments: ");
    for(int i = 0; i < argc; i++)
        printf("%s ", argv[i]);
    printf("\n");
    return 0;
}

#elif defined LAB_9_CASE_3

#include "userlib.h"

static int try_to_open(char* path, uint32 mode)
{
    int fd = sys_open(path, mode);
    if(fd < 0) {
        printf("open %s fail\n", path);
        while(1);
    }
    return fd;
}

static void try_to_mkdir(char* path)
{
    int ret = sys_mkdir(path);
    if(ret < 0) {
        printf("mkdir %s fail\n", path);
        while(1);
    }
}

static dirent_t dirents[10];
static uint32 dirlen;

static void try_to_print_dir(char* path, char* dirname)
{
    printf("%s ",dirname);
    int fd = try_to_open(path, MODE_READ);
    dirlen = sys_getdir(fd, dirents, sizeof(dirents));
    print_dirents(dirents, dirlen / sizeof(dirent_t));
    sys_close(fd);
}

int main(int argc, char* argv[])
{
    int ret = 0, fd = 0;

    // 输出根目录内容
    try_to_print_dir(".", "root");

    // 在根目录下创建workdir
    // 在workdir下创建student和teacher目录和hello.txt文件
    // 输出workdir的目录项
    try_to_mkdir("/workdir");
    try_to_mkdir("/workdir/student");
    try_to_mkdir("/workdir/teacher");
    fd = try_to_open("./workdir/hello.txt", MODE_CREATE | MODE_READ | MODE_WRITE);
    sys_close(fd);
    try_to_print_dir(".", "root");
    try_to_print_dir("/workdir", "workdir");

    // 修改当前目录项并测试修改是否生效
    ret = sys_chdir("./workdir/student");
    if(ret < 0) {
        printf("chdir fail\n");
        while(1);
    }
    try_to_print_dir("..", "workdir");
    try_to_print_dir("././../..","root");

    return 0;
}

#elif defined LAB_9_CASE_4

#include "userlib.h"

static int try_to_open(char* path, uint32 mode)
{
    int fd = sys_open(path, mode);
    if(fd < 0) {
        printf("open %s fail\n", path);
        while(1);
    }
    return fd;
}

int main(int argc, char* argv[])
{
    int ret = 0, fd = 0;

    char str[] = "hello wrold";
    char tmp[15];

    // 创建文件 + 简单读写 + 指针移动 + 文件状态检查

    fd = try_to_open("./hello.txt", MODE_WRITE | MODE_READ | MODE_CREATE);
    ret = sys_write(fd, strlen(str), str);
    printf("write_len = %d\n", ret);
    
    sys_lseek(fd, 11, LSEEK_SUB);
    ret = sys_read(fd, 10, tmp);
    tmp[ret] = 0;
    printf("read_len = %d read = %s\n", ret, tmp);

    sys_lseek(fd, 6, LSEEK_SET);
    ret = sys_read(fd, 10, tmp);
    tmp[ret] = 0;
    printf("read_len = %d read = %s\n", ret, tmp);

    fstat_t fstate;
    sys_fstat(fd, &fstate);
    print_filestate(&fstate);

    sys_close(fd);

    return 0;
}

#elif defined LAB_9_CASE_5

#include "userlib.h"

static int try_to_open(char* path, uint32 mode)
{
    int fd = sys_open(path, mode);
    if(fd < 0) {
        printf("open %s fail\n", path);
        while(1);
    }
    return fd;
}

static dirent_t dirents[10];
static uint32 dirlen;

static void try_to_print_dir(char* path, char* dirname)
{
    printf("%s ",dirname);
    int fd = try_to_open(path, MODE_READ);
    dirlen = sys_getdir(fd, dirents, sizeof(dirents));
    print_dirents(dirents, dirlen / sizeof(dirent_t));
    sys_close(fd);
}

int main(int argc, char* argv[])
{
    int ret = 0, fd = 0, new_fd = 0;
    char tmp1[10], tmp2[10];

    // 测试 sys_dup
    
    fd = sys_dup(STD_OUT);
    sys_write(fd, 16, "sys_dup success\n");
    sys_close(fd);

    // 测试 sys_link
    
    fd = try_to_open("hello.txt", MODE_READ | MODE_WRITE | MODE_CREATE);
    sys_write(fd, 12, "hello world\n");
    sys_lseek(fd, 6, LSEEK_SET);

    ret = sys_link("hello.txt", "world.txt");
    if(ret < 0) {
        printf("link fail\n");
        while(1);
    }

    new_fd = try_to_open("world.txt", MODE_READ);
    
    sys_read(fd, 5, tmp1);
    sys_read(new_fd, 5, tmp2);
    printf("%s %s\n", tmp1, tmp2);

    // 测试 sys_unlink
    
    try_to_print_dir(".", "root");

    fstat_t fstate;
    sys_fstat(fd, &fstate);
    print_filestate(&fstate);
    sys_unlink("world.txt");

    sys_fstat(fd, &fstate);
    print_filestate(&fstate);
    sys_unlink("hello.txt");

    sys_close(fd);
    sys_close(new_fd);

    try_to_print_dir(".", "root");

    return 0;
}

#else

#error "No pre-defined macro. Can not decide what to test or run"

#endif

#endif // __TESTCASES_H__
