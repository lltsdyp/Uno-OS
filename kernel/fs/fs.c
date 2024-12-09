#include "fs/fs.h"
#include "fs/buf.h"
#include "fs/bitmap.h"
#include "lib/str.h"
#include "lib/print.h"
#include "fs/inode.h"

// 超级块在内存的副本
super_block_t sb;

// 输出super_block的信息
static void sb_print()
{
    printf("super block information:\n");
    printf("magic = %x\n", sb.magic);
    printf("block size = %d\n", sb.block_size);
    printf("inode blocks = %d\n", sb.inode_blocks);
    printf("data blocks = %d\n", sb.data_blocks);
    printf("total blocks = %d\n", sb.total_blocks);
    printf("inode bitmap start = %d\n", sb.inode_bitmap_start);
    printf("inode start = %d\n", sb.inode_start);
    printf("data bitmap start = %d\n", sb.data_bitmap_start);
    printf("data start = %d\n", sb.data_start);
}

static char str[2*BLOCK_SIZE],tmp[2*BLOCK_SIZE];

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
    uint32 ret = 0;

    for(int i = 0; i < BLOCK_SIZE * 2; i++)
        str[i] = i;

    // 创建新的inode
    inode_t* nip = inode_create(FT_FILE, 0, 0);
    inode_lock(nip);
    
    // 第一次查看
    inode_print(nip);

    // 第一次写入
    ret = inode_write_data(nip, 0, BLOCK_SIZE / 2, str, false);
    assert(ret == BLOCK_SIZE / 2, "inode_write_data: fail, ret %d",(int)ret);

    // 第二次写入
    ret = inode_write_data(nip, BLOCK_SIZE / 2, BLOCK_SIZE + BLOCK_SIZE / 2, str + BLOCK_SIZE / 2, false);
    assert(ret == BLOCK_SIZE +  BLOCK_SIZE / 2, "inode_write_data: fail, ret %d",(int)ret);

    // 一次读取
    ret = inode_read_data(nip, 0, BLOCK_SIZE * 2, tmp, false);
    assert(ret == BLOCK_SIZE * 2, "inode_read_data: fail, ret %d",(int)ret);

    // 第二次查看
    inode_print(nip);
    
    inode_unlock_free(nip);

    // 测试
    if(strncmp(tmp, str,2*BLOCK_SIZE) == 0)
        printf("success\n");
    else
        printf("fail\n");
    // END TEST 8-2

    // 检查超级块的magic值是否正确，确保文件系统没有损坏
    assert(sb.magic == FS_MAGIC, "fs_init: invalid super block magic number");
    assert(sb.block_size == BLOCK_SIZE, "fs_init: invalid super block size");
}

// TODO!
