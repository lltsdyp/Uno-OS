#include "userlib.h"

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

    // // 测试向无效的文件描述符写入，期望失败
    // bytes_written = sys_write(fd, strlen(data), (uint64)data);
    // assert(bytes_written == -1);
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

int main()
{
    // printf("%s",longtext);
    // TEST SUITE 1
    test_sys_open();
    test_sys_close();
    test_sys_write();
    test_sys_read();
    test_sys_lseek();
    test_sys_dup();

    // TEST SUITE 2
    test_sys_mkdir();
    test_sys_getdir();
    test_sys_link();
    test_sys_unlink();

    // TEST SUITE 3
    test_pid();
    test_ppid();
    test_time();
    return 0;
}
