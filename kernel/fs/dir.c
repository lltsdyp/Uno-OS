#include "fs/fs.h"
#include "fs/buf.h"
#include "fs/inode.h"
#include "fs/dir.h"
#include "fs/bitmap.h"
#include "lib/str.h"
#include "lib/print.h"
#include "proc/cpu.h"
#include "fs/file.h"
#include "fs/dinode.h"

static bool check_unlink(inode_t* ip);

// 对目录文件的简化性假设: 每个目录文件只包括一个block
// 也就是每个目录下最多 BLOCK_SIZE / sizeof(dirent_t) = 32 个目录项

// 查询一个目录项是否在目录里
// 成功返回这个目录项的inode_num
// 失败返回INODE_NUM_UNUSED
// ps: 调用者需持有pip的锁
uint16 dir_search_entry(inode_t *pip, char *name)
{
    assert(sleeplock_holding(&(pip->slk)), "dir_search_entry: lock");

    if (pip->disk_inode.addrs[0] == 0) 
        return INODE_NUM_UNUSED;
    buf_t *buf = buf_read(pip->disk_inode.addrs[0]); // 读取目录block
    dirent_t *de;

    // 遍历目录项
    for (uint32 offset = 0; offset < BLOCK_SIZE; offset += sizeof(dirent_t))
    {
        de = (dirent_t *)(buf->data + offset);

        if (de->name[0] == 0) // 检测空目录项
            continue;

        // 匹配目录名
        if (strncmp(de->name, name, DIR_NAME_LEN) == 0)
        {
            buf_release(buf);
            return de->inode_num; // 返回匹配条目的inode_num
        }
    }

    buf_release(buf);
    return INODE_NUM_UNUSED; // 未找到
}

// 在pip目录下添加一个目录项
// 成功返回这个目录项的偏移量 (同时更新pip->size)
// 失败返回BLOCK_SIZE (没有空间 或 发生重名)
// ps: 调用者需持有pip的锁
uint32 dir_add_entry(inode_t *pip, uint16 inode_num, char *name)
{
    assert(sleeplock_holding(&pip->slk), "dir_add_entry: lock");

    // 检查目录中是否已存在该名称
    if (dir_search_entry(pip, name) != INODE_NUM_UNUSED)
        return BLOCK_SIZE;  // 目录项已存在

    dirent_t *de;
    if(!pip->disk_inode.addrs[0])
        pip->disk_inode.addrs[0] = bitmap_alloc_block();
    assert(pip->disk_inode.addrs[0], "dir_add_entry: no free block");
    buf_t *buf = buf_read(pip->disk_inode.addrs[0]);

    // 查找第一个空的目录项
    uint32 offset;
    for (offset = 0; offset < BLOCK_SIZE; offset += sizeof(dirent_t))
    {
        de = (dirent_t *)(buf->data + offset);
        if (de->name[0] == 0)  // 空目录项
            break;
    }

    if (offset == BLOCK_SIZE)
    {
        buf_release(buf);
        return BLOCK_SIZE;  // 没有空余空间
    }

    // 填充新的目录项
    memcpy(de->name, name, DIR_NAME_LEN);
    de->inode_num = inode_num;

    // 更新目录大小
    pip->disk_inode.size += sizeof(dirent_t);

    // 写回到目录
    buf_write(buf);
    buf_release(buf);
    return offset;
}

// 在pip目录下删除一个目录项
// 成功返回这个目录项的inode_num
// 失败返回INODE_NUM_UNUSED
// ps: 调用者需持有pip的锁
uint16 dir_delete_entry(inode_t *pip, char *name)
{
    assert(sleeplock_holding(&pip->slk), "dir_delete_entry: lock");

    dirent_t *de;
    buf_t *buf = buf_read(pip->disk_inode.addrs[0]);

    //  遍历目录项
    for (uint32 offset = 0; offset < BLOCK_SIZE; offset += sizeof(dirent_t))
    {
        de = (dirent_t *)(buf->data + offset);

        if (de->name[0] == 0) // 检测空目录项
            continue;

        // 匹配目录名
        if ((strncmp(de->name, name, DIR_NAME_LEN)) == 0)
        {
            de->name[0] = 0;
            de->inode_num = INODE_NUM_UNUSED;
            pip->disk_inode.size -= sizeof(dirent_t);

            buf_write(buf);
            buf_release(buf);
            return de->inode_num; // 返回匹配条目的inode_num
        }
    }

    buf_release(buf);
    return INODE_NUM_UNUSED;
}

// 输出一个目录下的所有有效目录项
// for debug
// ps: 调用者需持有pip的锁
void dir_print(inode_t *pip)
{
    assert(sleeplock_holding(&pip->slk), "dir_print: lock");

    printf("\ninode_num = %d dirents:\n", pip->inode_num);

    dirent_t *de;
    buf_t *buf = buf_read(pip->disk_inode.addrs[0]);
    for (uint32 offset = 0; offset < BLOCK_SIZE; offset += sizeof(dirent_t))
    {
        de = (dirent_t *)(buf->data + offset);
        if (de->name[0] != 0)
            printf("inum = %d dirent = %s\n", de->inode_num, de->name);
    }
    buf_release(buf);
}

/*----------------------- 路径(一串目录和文件) -------------------------*/

// Examples:
//   skipelem("a/bb/c", name) = "bb/c", setting name = "a"
//   skipelem("///a//bb", name) = "bb", setting name = "a"
//   skipelem("a", name) = "", setting name = "a"
//   skipelem("", name) = skipelem("////", name) = 0
static char *skip_element(char *path, char *name)
{
    while(*path == '/') path++;
    if(*path == '\0') return NULL;

    char *s = path;
    while (*path != '/' && *path != '\0')
        path++;

    int len = path - s;
    if (len >= DIR_NAME_LEN) {
        memcpy(name, s, DIR_NAME_LEN - 1);
        name[DIR_NAME_LEN - 1] = '\0';
    } else {
        memcpy(name, s, len);
        name[len] = '\0';
    }
    
    while (*path == '/')
        path++;

    return path;
}

// 查找路径path对应的inode (find_parent = false)
// 查找路径path对应的inode的父节点 (find_parent = true)
// 供两个上层函数使用
// 失败返回NULL
static inode_t* search_inode(char* path, char* name, bool find_parent)
{
    inode_t *ip, *next;
    uint16 inode_num;
    
    ip = inode_get(INODE_ROOT);

    while ((path = skip_element(path, name)) != 0)
    {
        inode_lock(ip);
        if (ip->disk_inode.type != FT_DIR)
        {
            inode_unlock_free(ip);
            return NULL;  // 不是目录
        }

        // 如果只需要查找父节点，则返回当前节点
        if (find_parent && *path == '\0'){
            inode_unlock(ip);
            return ip;
        }

        inode_num = dir_search_entry(ip, name);
        if((next = inode_get(inode_num)) == NULL){
            inode_unlock_free(ip);
            return NULL;
        }

        // 进入下一级目录
        inode_unlock_free(ip);  // 解锁并释放当前 inode 的引用
        ip = next;
    }

    if (find_parent)
    {
        inode_free(ip);
        return NULL;
    }
    if(ip->inode_num==INODE_NUM_UNUSED)
        return NULL;
    return ip;  // 返回目标 inode
}

// 找到path对应的inode
inode_t* path_to_inode(char* path)
{
    char name[DIR_NAME_LEN];
    return search_inode(path, name, false);
}

// 找到path对应的inode的父节点
// path最后的目录名放入name指向的空间
inode_t* path_to_pinode(char* path, char* name)
{
    return search_inode(path, name, true);
}

// 如果path对应的inode存在则返回inode
// 如果path对应的inode不存在则创建inode
// 失败返回NULL
inode_t* path_create_inode(char* path, uint16 type, uint16 major, uint16 minor)
{
    char name[DIR_NAME_LEN];
    inode_t *dp = path_to_pinode(path, name);  // 查找父目录 inode和文件名
    if (dp == 0) 
        return 0;  // 父目录不存在

    inode_lock(dp);

    inode_t *ip = path_to_inode(path);
    if (ip != NULL && ip->inode_num!=INODE_NUM_UNUSED)
    {
        inode_unlock_free(dp);
        inode_lock(ip);
        if(type == FD_FILE && (ip->disk_inode.type == FD_FILE || ip->disk_inode.type == FD_DEVICE))
            return ip;
        inode_unlock_free(ip);
        return 0;
    }

    // 创建一个新的 inode
    ip = (inode_t*)inode_create(type, major, minor);
    if (ip == NULL || ip->inode_num == INODE_NUM_UNUSED) {
        inode_unlock_free(dp);
        return NULL;  // inode 分配失败
    }
    inode_lock(ip);
    inode_rw(ip, 1);

    if(type == FD_DIR)
    {
        dp->disk_inode.nlink++;
        inode_rw(dp, 1);

        assert((dir_add_entry(ip, ip->inode_num, ".") != BLOCK_SIZE && 
            dir_add_entry(ip, dp->inode_num, "..") != BLOCK_SIZE), 
            "path_creat_inode: . and .. fail");
    }

    assert(dir_add_entry(dp, ip->inode_num, name) != BLOCK_SIZE, "path_creat_inode: add ip to pip failed");
    inode_unlock_free(dp);
    return ip;
}

uint32 path_link(char* old_path, char* new_path)
{
    char name[DIR_NAME_LEN];
    inode_t *ip, *dp;

    // 查找旧路径对应的 inode
    ip = path_to_inode(old_path);  
    if (ip == NULL || ip->inode_num == INODE_NUM_UNUSED) 
        return -1;  // 旧路径对应的文件不存在

    inode_lock(ip);
    if (ip->disk_inode.type == FD_DIR) {  // 目录不能被链接
        inode_unlock_free(ip);
        return -1;
    }

    ip->disk_inode.nlink++;  // 增加链接数
    inode_rw(ip, 1);
    inode_unlock(ip);

    // 查找新路径的父目录 inode
    dp = path_to_pinode(new_path, name);
    if (dp == NULL) {
        inode_lock(ip);
        ip->disk_inode.nlink--;
        inode_rw(ip, 1);
        inode_unlock_free(ip);
        return -1;  // 新路径的父目录不存在
    }

    inode_lock(dp);

    // 在新路径的父目录下添加旧文件的目录项
    if (dir_add_entry(dp, ip->inode_num, name) == BLOCK_SIZE) {
        inode_unlock_free(dp);
        // 失败，恢复旧文件的链接数
        inode_lock(ip);
        ip->disk_inode.nlink--;
        inode_unlock_free(ip);
        return -1;
    }

    inode_unlock_free(dp);
    inode_free(ip);
    return 0;  // 链接成功
}

// 文件删除链接
// 成功返回0，失败返回-1
uint32 path_unlink(char* path)
{
    char name[DIR_NAME_LEN];
    inode_t *ip, *dp;
    uint16 inode_num;

    // 查找路径对应的父目录和文件名
    if ((dp = path_to_pinode(path, name)) == NULL)
        return -1; 

    inode_lock(dp);  // 锁住父目录

    // 不允许删除 "." 或 ".." 目录
    if (strncmp(name, ".", DIR_NAME_LEN) == 0 || strncmp(name, "..", DIR_NAME_LEN) == 0) {
        inode_unlock_free(dp);
        return -1;
    }

    // 查找目录项，得到对应的 inode
    if ((inode_num = dir_search_entry(dp, name)) == INODE_NUM_UNUSED) {
        inode_unlock_free(dp);
        return -1;  // 目录项不存在
    }
    
    ip = inode_get(inode_num);
    inode_lock(ip);      // 锁住 inode
    assert(ip->disk_inode.nlink >= 1, "path_unlink: nlink < 1");

    // 如果是目录，但是目录不为空
    if ((ip->disk_inode.type == FT_DIR) && !check_unlink(ip)) {
        inode_unlock_free(ip);
        inode_unlock_free(dp);
        return -1;
    }


    // 删除目录项
    inode_num = dir_delete_entry(dp, name);
    if (inode_num == INODE_NUM_UNUSED) {
        inode_unlock_free(ip);
        return -1;  // 删除失败
    }

    if(ip->disk_inode.type == FD_DIR)
    {
        dp->disk_inode.nlink--;
        inode_rw(dp, 1);
    }
    inode_unlock_free(dp);

    // 递减 inode 的链接数
    ip->disk_inode.nlink--;
    inode_rw(ip, 1);
    inode_unlock_free(ip);
    
    return 0;  // 删除成功
}

// 检查一个unlink操作是否合理
// 调用者需要持有ip的锁
// 在path_unlink()中调用
// 检查目录是否为空
static bool check_unlink(inode_t* ip)
{
    assert(sleeplock_holding(&ip->slk), "check_unlink: slk");

    uint8 tmp[sizeof(dirent_t) * 3];
    uint32 read_len;
    
    read_len = dir_get_entries(ip, sizeof(dirent_t) * 3, tmp, false);
    
    if(read_len == sizeof(dirent_t) * 3) {
        return false;
    } else if(read_len == sizeof(dirent_t) * 2) {
        return true;
    } else {
        panic("check_unlink: read_len");
        return false;
    }
}

// 把目录下的有效目录项复制到dst(dst长度为len)
// 返回读到的字节数(sizeof(dirent_t)*n)
// 调用者需要持有pip的锁
uint32 dir_get_entries(inode_t *pip, uint32 len, void* dst, bool user)
{
    assert(sleeplock_holding(&pip->slk), "dir_get_entries: lock");
    buf_t *buf;
    dirent_t *de;
    uint32 offset, bytes_read;

    // 如果目录为空，直接返回0
    if (pip->disk_inode.addrs[0] == 0) 
        return 0;

    buf = buf_read(pip->disk_inode.addrs[0]);  // 读取目录的第一个数据块
    bytes_read = 0;

    // 遍历目录项并将有效的目录项复制到 dst
    for (offset = 0; offset < BLOCK_SIZE && bytes_read < len; offset += sizeof(dirent_t))
    {
        de = (dirent_t *)(buf->data + offset);

        if (de->name[0] == 0) {
            continue;  // 空目录项跳过
        }

        // 将有效的目录项复制到 dst 中
        uint32 copy_size = sizeof(dirent_t);
        if (bytes_read + copy_size > len) {
            // 如果剩余空间不足以拷贝一个完整的 dirent_t，退出
            break;
        }

        // 复制目录项到目标地址
        memcpy((uint8 *)dst + bytes_read, de, copy_size);
        bytes_read += copy_size;
    }

    buf_release(buf);  // 释放缓冲区

    return bytes_read;  // 返回实际读取的字节数
}

// 改变进程里存储的当前目录
// 成功返回0，失败返回-1
uint32 dir_change(char *path)
{    
    inode_t *ip = path_to_inode(path);
    if ((ip == NULL) || (ip->inode_num == INODE_NUM_UNUSED)) 
        return -1; 

    inode_lock(ip);  // 锁住找到的 inode
    if (ip->disk_inode.type != FT_DIR) {
        inode_unlock_free(ip);
        return -1;
    }

    proc_t *p = myproc();
    p->cwd = ip;  // 更新进程的当前工作目录

    inode_unlock_free(ip); 
    return 0;
}