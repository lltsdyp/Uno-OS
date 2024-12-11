#include "fs/fs.h"
#include "fs/buf.h"
#include "fs/inode.h"
#include "fs/dir.h"
#include "fs/bitmap.h"
#include "lib/str.h"
#include "lib/print.h"
#include "proc/cpu.h"

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
    if(*path == 0) return 0;

    char *s = path;
    while (*path != '/' && *path != 0)
        path++;

    int len = path - s;
    if (len >= DIR_NAME_LEN) {
        memcpy(name, s, DIR_NAME_LEN);
    } else {
        memcpy(name, s, len);
        name[len] = 0;
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
    proc_t *p = myproc();
    
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