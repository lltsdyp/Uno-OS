#include "userlib.h"

static int try_to_open(char* path, uint32 mode)
{
    printf("open start\n");
    int fd = sys_open(path, mode);
    if(fd < 0) {
        printf("open %s fail\n", path);
        while(1);
    }
    printf("open finish\n");
    return fd;
}

static void try_to_mkdir(char* path)
{
    printf("mkdir start\n");
    int ret = sys_mkdir(path);
    if(ret < 0) {
        printf("mkdir %s fail\n", path);
        while(1);
    }
    printf("mkdir finish\n");
}

static dirent_t dirents[10];
static uint32 dirlen;

static void try_to_print_dir(char* path, char* dirname)
{
    printf("Print dir start\n");
    printf("%s \n",dirname);
    int fd = try_to_open(path, MODE_READ);
    dirlen = sys_getdir(fd, dirents, sizeof(dirents));
    print_dirents(dirents, dirlen / sizeof(dirent_t));
    sys_close(fd);
    printf("Print dir finish\n");
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