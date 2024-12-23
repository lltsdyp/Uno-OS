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