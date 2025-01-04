#include "userlib.h"

#define MMAP_BEGIN 0x0000003ffe03e000

void assert(int condition, const char* message) {
    if (!condition) {
        printf("%s failed\n", message);
        while (1); // 或者使用其他错误处理机制
    } else {
        printf("%s passed\n", message);
    }
}

void test_sys_open() {
    // 测试创建一个新文件
    int fd = sys_open("/newfile.txt", MODE_WRITE | MODE_CREATE);
    assert(fd >= 0,"sys_open:test1");
    sys_close(fd);

    // 测试打开一个存在的文件
    fd = sys_open("/newfile.txt", MODE_READ);
    assert(fd >= 0,"sys_open:test2");
    sys_close(fd);
}

void test_sys_close() {
    // 测试关闭一个有效的文件描述符
    int fd = sys_open("/newfile.txt", MODE_READ);
    assert(fd >= 0,"sys_close:test1");
    assert(sys_close(fd) == 0,"sys_close:test2");

    // 测试关闭一个无效的文件描述符，期望失败
    assert(sys_close(fd) == -1,"sys_close:test3");
}

void test_sys_read() {
    // 测试从文件中读取数据
    int fd = sys_open("/testfile.txt", MODE_READ);
    sys_lseek(fd, 0, LSEEK_SET);
    assert(fd >= 0,"sys_read:test1");
    char buffer[100];
    int bytes_read = sys_read(fd, 100, (void *)buffer);
    assert(bytes_read > 0,"sys_read:test2");
    sys_close(fd);

    // // 测试从无效的文件描述符读取，期望失败
    // bytes_read = sys_read(fd, 100, (uint64)buffer);
    // assert(bytes_read == -1,"sys_read:test3");
}

void test_sys_write() {
    // 测试向文件写入数据
    int fd = sys_open("/testfile.txt", MODE_WRITE | MODE_CREATE);
    assert(fd >= 0,"sys_write:test1");
    const char* data = "Hello, world!!!!!!!!!!!";
    int bytes_written = 0;
    for(int i=0;i<1000;++i)
        bytes_written += sys_write(fd, strlen(data), (void *)data);
    assert(bytes_written == strlen(data)*1000,"sys_write:test2");
    sys_close(fd);
}


void test_sys_lseek() {
    // 测试设置文件偏移量 (LSEEK_SET)
    int fd = sys_open("/testfile.txt", MODE_READ|MODE_WRITE);
    assert(fd >= 0, "test_sys_lseek:open1");
    int offset = sys_lseek(fd, 10, LSEEK_SET);
    assert(offset == 10, "test_sys_lseek:lseek1");

    // 测试添加到当前偏移量 (LSEEK_ADD)
    offset = sys_lseek(fd, 5, LSEEK_ADD);
    assert(offset == 15, "test_sys_lseek:lseek2");

    // 测试从当前偏移量减去 (LSEEK_SUB)
    offset = sys_lseek(fd, 5, LSEEK_SUB);
    assert(offset == 10, "test_sys_lseek:lseek3");

    sys_close(fd);
}

void test_sys_dup() {
    // 测试复制文件描述符
    int fd = sys_open("/testfile.txt", MODE_READ);
    assert(fd >= 0,"sys_dup:test1");
    int new_fd = sys_dup(fd);
    assert(new_fd >= 0,"sys_dup:test2");
    sys_close(fd);
    sys_close(new_fd);
}

void test_sys_fstat() {
    // 测试获取文件信息
    int fd = sys_open("/testfile.txt", MODE_READ);
    assert(fd >= 0,"sys_fstat:test1");
    fstat_t st;
    int result = sys_fstat(fd, &st);
    assert(result == 0,"sys_fstat:test2");
    sys_close(fd);
}

void test_sys_getdir() {
    // 测试读取目录项
    int fd = sys_open("/", MODE_READ);
    assert(fd >= 0,"sys_getdir:test1");
    dirent_t dirent[10];
    int bytes_read = sys_getdir(fd, dirent, sizeof(dirent));
    assert(bytes_read > 0,"sys_getdir:test2");
    sys_close(fd);
}

void test_sys_mkdir() {
    // 测试创建目录
    int result = sys_mkdir("/newdir");
    assert(result == 0,"sys_mkdir:test1");


    result=sys_mkdir("/newdir/subdir");
    assert(result==0,"sys_mkdir:test2");

    result=sys_mkdir("/newdir/subdir/subsubdir");
    assert(result==0,"sys_mkdir:test3");
}

void test_sys_chdir() {
    // 测试切换到存在的目录
    int result = sys_chdir("/newdir");
    assert(result == 0,"sys_chdir:test1");
}

void test_sys_link() {
    // 测试创建文件链接
    int result = sys_link("/testfile.txt", "/testfile_link.txt");
    assert(result == 0,"sys_link:test1");
}

void test_sys_unlink() {
    // 测试删除文件链接
    int result = sys_unlink("/testfile_link.txt");
    assert(result == 0,"sys_unlink:test1");
}

void test_pid()
{
    // 因为proczero的唯一任务就是加载当前进程，因此我们可以通过pid=2来判断是否成功
    assert(sys_pid()==2,"test_pid:test1");
}

void test_ppid()
{
    assert(sys_ppid() == 1, "test_ppid:test1");
}

void test_time()
{
    uint64 tick=sys_time();
    printf("time:%d\n",(int)tick);
}

void test_sleep()
{
    uint64 start=sys_time();
    sys_sleep(5);
    uint64 end=sys_time();
    assert(end-start>=5,"test_sleep:test1");
}

void test_sys_brk() {
    // 测试查询堆顶
    uint64 current_heap_top = sys_brk(0);
    assert(current_heap_top != 0, "sys_brk:test1");

    // 测试扩展堆
    uint64 new_heap_top = sys_brk(current_heap_top + 4096);
    assert(new_heap_top == current_heap_top + 4096, "sys_brk:test2");

    // 测试收缩堆
    new_heap_top = sys_brk(new_heap_top - 4096);
    assert(new_heap_top == current_heap_top, "sys_brk:test3");
}

void test_sys_mmap() {
    // 测试内存映射，由内核选择起始地址
    uint64 start = sys_mmap(0, 4096);
    assert(start != (uint64)-1, "sys_mmap:test1");

    // 测试内存映射，指定起始地址
    start = sys_mmap(MMAP_BEGIN+8192, 8192);
    assert(start == MMAP_BEGIN+8192, "sys_mmap:test2");
}

void test_sys_munmap() {
    // 测试取消内存映射
    uint64 start = sys_mmap(0, 4096);
    assert(start != (uint64)-1, "sys_munmap:test1");
    assert(sys_munmap(start, 4096) == 0, "sys_munmap:test2");

    assert(sys_munmap(MMAP_BEGIN+8192, 8192) == 0, "sys_munmap:test3");
}

void test_sys_pipe() {
    int pipefd[2];
    char buffer[15];
    const char* message = "Hello, pipe!";


    // 测试创建管道
    assert(sys_pipe(pipefd) == 0, "sys_pipe:test1");

    // 测试向管道写入数据
    int bytes_written = sys_write(pipefd[1], strlen(message), (void *)message);
    assert(bytes_written == strlen(message), "sys_pipe:test2");

    // 测试从管道读取数据
    int bytes_read = sys_read(pipefd[0], 100, (void *)buffer);
    assert(bytes_read == strlen(message), "sys_pipe:test3");
    assert(strncmp(buffer, message,12) == 0, "sys_pipe:test4");

    // 测试关闭管道文件描述符
    assert(sys_close(pipefd[0]) == 0, "sys_pipe:test5");
    assert(sys_close(pipefd[1]) == 0, "sys_pipe:test6");
}

int main()
{
    sys_clear();
    sys_curhide();
    // sys_forecolor(255,255,0);
    test_sys_open();
    test_sys_close();
    test_sys_write();
    sys_conreset();
    // sys_backcolor(0,255,0);
    test_sys_read();
    test_sys_lseek();
    test_sys_dup();
    test_sys_mkdir();
    test_sys_getdir();
    test_sys_link();
    test_sys_unlink();
    test_pid();
    test_ppid();
    test_time();
    test_sleep();
    test_sys_brk();
    test_sys_mmap();
    test_sys_munmap();
    test_sys_pipe();
    sys_curshow();
    sys_halt();

    return 0;
}
