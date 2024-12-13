#include "fs/fs.h"
#include "fs/buf.h"
#include "fs/bitmap.h"
#include "lib/str.h"
#include "lib/print.h"
#include "fs/inode.h"
#include "fs/dir.h"
#include "fs/dinode.h"

// 超级块在内存的副本
super_block_t sb;

// // 输出super_block的信息
// static void sb_print()
// {
//     printf("super block information:\n");
//     printf("magic = %x\n", sb.magic);
//     printf("block size = %d\n", sb.block_size);
//     printf("inode blocks = %d\n", sb.inode_blocks);
//     printf("data blocks = %d\n", sb.data_blocks);
//     printf("total blocks = %d\n", sb.total_blocks);
//     printf("inode bitmap start = %d\n", sb.inode_bitmap_start);
//     printf("inode start = %d\n", sb.inode_start);
//     printf("data bitmap start = %d\n", sb.data_bitmap_start);
//     printf("data start = %d\n", sb.data_start);
// }

static char str[BLOCK_SIZE],tmp[BLOCK_SIZE],empty[BLOCK_SIZE];

// 文件系统初始化
void fs_init()
{
    buf_init();
    inode_init();

    buf_t* buf = buf_read(SB_BLOCK_NUM);
    memcpy(&sb, buf->data, sizeof(sb));
    buf_release(buf);


    // // TEST 8-1
    // // 原本就存在的inode
    // sb_print();
    // inode_t* ip = inode_get(INODE_ROOT);
    // inode_lock(ip);
    // inode_print(ip);
    // inode_unlock(ip);
    // bitmap_print(sb.inode_bitmap_start);

    // // 创建新的inode
    // inode_t* nip = inode_create(FT_FILE, 0, 0);
    // inode_lock(nip);
    // inode_print(nip);
    // inode_unlock(nip);
    // bitmap_print(sb.inode_bitmap_start);

    // // 尝试删除inode
    // inode_lock(nip);
    // nip->disk_inode.nlink = 0;
    // inode_unlock_free(nip);
    // bitmap_print(sb.inode_bitmap_start);
    // // END TEST 8-1

    // TEST 8-2
    // uint32 ret = 0;

    // for(int i = 0; i < BLOCK_SIZE * 2; i++)
    //     str[i] = i;

    // // 创建新的inode
    // inode_t* nip = inode_create(FT_FILE, 0, 0);
    // inode_lock(nip);
    
    // // 第一次查看
    // inode_print(nip);

    // // 第一次写入
    // ret = inode_write_data(nip, 0, BLOCK_SIZE / 2, str, false);
    // assert(ret == BLOCK_SIZE / 2, "inode_write_data: fail, ret %d",(int)ret);

    // // 第二次写入
    // ret = inode_write_data(nip, BLOCK_SIZE / 2, BLOCK_SIZE + BLOCK_SIZE / 2, str + BLOCK_SIZE / 2, false);
    // assert(ret == BLOCK_SIZE +  BLOCK_SIZE / 2, "inode_write_data: fail, ret %d",(int)ret);

    // // 一次读取
    // ret = inode_read_data(nip, 0, BLOCK_SIZE * 2, tmp, false);
    // assert(ret == BLOCK_SIZE * 2, "inode_read_data: fail, ret %d",(int)ret);

    // // 第二次查看
    // inode_print(nip);
    
    // inode_unlock_free(nip);

    // // 测试
    // if(strncmp(tmp, str,2*BLOCK_SIZE) == 0)
    //     printf("success\n");
    // else
    //     printf("fail\n");
    // END TEST 8-2

    // TEST 8-3
    uint32 ret = 0;

    for(int i = 0; i < BLOCK_SIZE; i++) {
        str[i] = i;
        empty[i] = 0;
    }

    // 创建新的inode
    inode_t* nip = inode_create(FT_FILE, 0, 0);
    inode_lock(nip);
    
    // 第一次查看
    inode_print(nip);
    bitmap_print(sb.data_bitmap_start);

    uint32 max_blocks =  N_ADDRS_1 + N_ADDRS_2 * ENTRY_PER_BLOCK + 2 * ENTRY_PER_BLOCK;
    // uint32 max_blocks =  N_ADDRS_1 + 1;

    for(uint32 i = 0; i < max_blocks; i++)
    {
        ret = inode_write_data(nip, i * BLOCK_SIZE, BLOCK_SIZE, str, false);
        assert(ret == BLOCK_SIZE, "inode_write_data fail, ret %d",ret);
    }
    ret = inode_write_data(nip, (max_blocks - 2) * BLOCK_SIZE, BLOCK_SIZE, empty, false);
    assert(ret == BLOCK_SIZE, "inode_write_data fail, ret %d",ret);
    
    // 第二次查看
    inode_print(nip);
    get_free_buf();

    // 区域-1
    ret = inode_read_data(nip, BLOCK_SIZE, BLOCK_SIZE, tmp, false);
    assert(ret == BLOCK_SIZE, "inode_read_data fail");
    assert(strncmp(tmp, str, BLOCK_SIZE) == 0, "check-1 fail"); 
    printf("check-1 success\n");

    // 区域-2
    ret = inode_read_data(nip, N_ADDRS_1 * BLOCK_SIZE, BLOCK_SIZE, tmp, false);
    assert(ret == BLOCK_SIZE, "inode_read_data fail");
    assert(strncmp(tmp, str, BLOCK_SIZE) == 0, "check-2 fail");
    printf("check-2 success\n");

    // 区域-3
    ret = inode_read_data(nip, (max_blocks - 2) * BLOCK_SIZE, BLOCK_SIZE, tmp, false);
    assert(ret == BLOCK_SIZE, "inode_read_data fail");
    assert(strncmp(tmp, empty, BLOCK_SIZE) == 0, "check-2 fail");
    printf("check-3 success\n");

    // 释放inode管理的所有data block
    inode_free_data(nip);
    printf("free success\n");

    // 第三次观察
    inode_print(nip);
    bitmap_print(sb.data_bitmap_start);

    inode_unlock_free(nip);
    // END TEST 8-3

    // TEST 8-4

    // 获取根目录
    // inode_t* ip = inode_alloc(INODE_ROOT);
    // inode_t* ip = inode_get(INODE_ROOT);    
    // inode_lock(ip);

    // // 第一次查看
    // dir_print(ip);
    
    // // add entry
    // dir_add_entry(ip, 1, "a.txt");
    // dir_add_entry(ip, 2, "b.txt");
    // dir_add_entry(ip, 3, "c.txt");
    
    // // 第二次查看
    // dir_print(ip);

    // // 第一次检查
    // assert(dir_search_entry(ip, "b.txt") == 2, "error-1");

    // // delete entry
    // dir_delete_entry(ip, "a.txt");
   
    // // 第三次查看
    // dir_print(ip);
    
    // // add entry
    // dir_add_entry(ip, 1, "d.txt");    
    
    // // 第四次查看
    // dir_print(ip);
    
    // // 第二次检查
    // assert(dir_add_entry(ip, 4, "d.txt") == BLOCK_SIZE, "error-2");
    
    // inode_unlock(ip);

    // printf("over");
    // END TEST 8-4

    // TEST 8-5
    // 创建inode
    // inode_t* ip = inode_get(INODE_ROOT);
    // inode_t* ip_1 = inode_create(FT_DIR, 0, 0);
    // inode_t* ip_2 = inode_create(FT_DIR, 0, 0);
    // inode_t* ip_3 = inode_create(FT_FILE, 0, 0);

    // // 上锁
    // inode_lock(ip);
    // inode_lock(ip_1);
    // inode_lock(ip_2);
    // inode_lock(ip_3);

    // // 创建目录
    // dir_add_entry(ip, ip_1->inode_num, "user");
    // dir_add_entry(ip_1, ip_2->inode_num, "work");
    // dir_add_entry(ip_2, ip_3->inode_num, "hello.txt");
    
    // // 填写文件
    // inode_write_data(ip_3, 0, 11, "hello world", false);

    // // 解锁
    // inode_unlock(ip_3);
    // inode_unlock(ip_2);
    // inode_unlock(ip_1);
    // inode_unlock(ip);

    // // something wrong
    // // 路径查找
    // char* path = "/user/work/hello.txt";
    // char name[DIR_NAME_LEN];
    // inode_t* tmp_1 = path_to_pinode(path, name);
    // inode_t* tmp_2 = path_to_inode(path);

    // assert(tmp_1 != NULL, "tmp1 = NULL");
    // assert(tmp_2 != NULL, "tmp2 = NULL");
    // printf("\nname = %s\n", name);

    // // 输出 tmp_1 的信息
    // inode_lock(tmp_1);
    // inode_print(tmp_1);
    // inode_unlock_free(tmp_1);

    // // 输出 tmp_2 的信息
    // inode_lock(tmp_2);
    // inode_print(tmp_2);
    // char str[12];
    // str[11] = 0;
    // inode_read_data(tmp_2, 0, tmp_2->disk_inode.size, str, false);
    // printf("read: %s\n", str);
    // inode_unlock_free(tmp_2);

    // printf("over");

    // END TEST 8-5

    // 检查超级块的magic值是否正确，确保文件系统没有损坏
    assert(sb.magic == FS_MAGIC, "fs_init: invalid super block magic number");
    assert(sb.block_size == BLOCK_SIZE, "fs_init: invalid super block size");
}

// TODO!
