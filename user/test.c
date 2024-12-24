#include "userlib.h"

void assert(int condition, const char* message) {
    if (!condition) {
        printf("%s failed\n", message);
        while (1); // 或者使用其他错误处理机制
    } else {
        printf("%s passed\n", message);
    }
}
// const char *longtext="Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the \"Software\"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:\n\nThe above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.\n\nTHE SOFTWARE IS PROVIDED \"AS IS\", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the \"Software\"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:\n\nThe above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.\n\nTHE SOFTWARE IS PROVIDED \"AS IS\", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the \"Software\"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:\n\nThe above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.\n\nTHE SOFTWARE IS PROVIDED \"AS IS\", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the \"Software\"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:\n\nThe above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.\n\nTHE SOFTWARE IS PROVIDED \"AS IS\", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.";

void test_sys_open() {

    // // 测试打开一个不存在的文件，期望失败
    // int fd = sys_open("/nonexistentfile.txt", MODE_READ);
    // assert(fd == -1,"sys_open:test1");

    // 测试创建一个新文件
    int fd = sys_open("/newfile.txt", MODE_WRITE | MODE_CREATE);
    assert(fd >= 0,"sys_open:test2");
    sys_close(fd);

    // 测试打开一个存在的文件
    fd = sys_open("/newfile.txt", MODE_READ);
    assert(fd >= 0,"sys_open:test3");
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
    int bytes_written = sys_write(fd, strlen(data), (void *)data);
    assert(bytes_written == strlen(data),"sys_write:test2");
    // TODO
    // bytes_written=sys_write(fd, 1048, (void *)longtext);
    // printf("%d\n",strlen(longtext));
    // assert(bytes_written == 1048,"sys_write:test3");
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

    // // 测试无效的文件描述符，期望失败
    // offset = sys_lseek(fd, 10, SEEK_SET);
    // assert(offset == -1, "test_sys_lseek:lseek4");
}

void test_sys_dup() {
    // 测试复制文件描述符
    int fd = sys_open("/testfile.txt", MODE_READ);
    assert(fd >= 0,"sys_dup:test1");
    int new_fd = sys_dup(fd);
    assert(new_fd >= 0,"sys_dup:test2");
    sys_close(fd);
    sys_close(new_fd);

    // // 测试无效的文件描述符，期望失败
    // new_fd = sys_dup(fd);
    // assert(new_fd == -1);
}

void test_sys_fstat() {
    // 测试获取文件信息
    int fd = sys_open("/testfile.txt", MODE_READ);
    assert(fd >= 0,"sys_fstat:test1");
    fstat_t st;
    int result = sys_fstat(fd, &st);
    assert(result == 0,"sys_fstat:test2");
    sys_close(fd);

//     // 测试无效的文件描述符，期望失败
//     result = sys_fstat(fd, (uint64)&st);
//     assert(result == -1);
}

void test_sys_getdir() {
    // 测试读取目录项
    int fd = sys_open("/", MODE_READ);
    assert(fd >= 0,"sys_getdir:test1");
    dirent_t dirent[10];
    int bytes_read = sys_getdir(fd, dirent, sizeof(dirent));
    assert(bytes_read > 0,"sys_getdir:test2");
    sys_close(fd);

    // 测试无效的文件描述符，期望失败
    // bytes_read = sys_getdir(fd, (uint64)buffer, 1024);
    // assert(bytes_read == -1);
}

void test_sys_mkdir() {
    // 测试创建目录
    int result = sys_mkdir("/newdir");
    assert(result == 0,"sys_mkdir:test1");


    result=sys_mkdir("/newdir/subdir");
    assert(result==0,"sys_mkdir:test2");

    result=sys_mkdir("/newdir/subdir/subsubdir");
    assert(result==0,"sys_mkdir:test3");


    // 测试创建已存在的目录，期望失败
    // result = sys_mkdir("/newdir");
    // assert(result == -1);
}

void test_sys_chdir() {
    // 测试切换到存在的目录
    int result = sys_chdir("/newdir");
    assert(result == 0,"sys_chdir:test1");

    // // 测试切换到不存在的目录，期望失败
    // result = sys_chdir("/nonexistentdir");
    // assert(result == -1);
}

void test_sys_link() {
    // 测试创建文件链接
    int result = sys_link("/testfile.txt", "/testfile_link.txt");
    assert(result == 0,"sys_link:test1");

    // // 测试创建已存在的链接，期望失败
    // result = sys_link("/testfile.txt", "/testfile_link.txt");
    // assert(result == -1);
}

void test_sys_unlink() {
    // 测试删除文件链接
    int result = sys_unlink("/testfile_link.txt");
    assert(result == 0,"sys_unlink:test1");

    // // 测试删除不存在的链接，期望失败
    // result = sys_unlink("/nonexistentlink.txt");
    // assert(result == -1);
}

int main()
{
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
    return 0;
}
