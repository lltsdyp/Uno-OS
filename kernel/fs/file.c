#include "fs/fs.h"
#include "fs/buf.h"
#include "fs/dir.h"
#include "fs/bitmap.h"
#include "fs/inode.h"
#include "fs/file.h"
#include "mem/vmem.h"
#include "proc/cpu.h"
#include "lib/print.h"
#include "dev/console.h"
#include "fs/pipe.h"

// 设备列表(读写接口)
dev_t devlist[N_DEV];

// ftable + 保护它的锁
#define N_FILE 32
file_t ftable[N_FILE];
spinlock_t lk_ftable;

// flags 可能取值
#define LSEEK_SET 0  // file->offset = offset
#define LSEEK_ADD 1  // file->offset += offset
#define LSEEK_SUB 2  // file->offset -= offset

// ftable初始化 + devlist初始化
void file_init()
{
    spinlock_init(&lk_ftable,"file table lock");
}

// alloc file_t in ftable
// 失败则panic
file_t* file_alloc()
{
    file_t *file=NULL;
    spinlock_acquire(&lk_ftable);

    for(int i=0;i<N_FILE;i++)
    {
        if(ftable[i].ref==0)
        {
            file=&ftable[i];
            file->ref=1;
            break;
        }
    }
    spinlock_release(&lk_ftable);
    assert(file!=NULL, "file_alloc: no free file block");
    return file;
}

// 创建设备文件(供proczero创建console)
file_t* file_create_dev(char* path, uint16 major, uint16 minor)
{
    inode_t *dev_inode=path_create_inode(path, FT_DEVICE, major, minor);
    inode_unlock_free(dev_inode);
    file_t *dev_file=file_alloc();
    dev_file->ip=dev_inode;
    dev_file->major=major;
    dev_file->readable=true;
    dev_file->writable=true;
    dev_file->type=FT_DEVICE;
    // TODO:是否有BUG？
    return dev_file;
}

// 打开一个文件
file_t* file_open(char* path, uint32 open_mode)
{
    inode_t *file_inode=NULL;

    // 第一步：从磁盘中找到（或创建）对应的inode，找不到返回-1

    // 创建文件
    if(open_mode & MODE_CREATE)
    {
        file_inode=path_create_inode(path, FT_FILE, 0, 0);
        if(!file_inode)
            return NULL;
    }
    // 打开文件
    else
    {
        file_inode=path_to_inode(path);
        if(!file_inode)
            return NULL;

        inode_lock(file_inode);
    }

    assert(!(file_inode->disk_inode.type == FT_DEVICE && file_inode->disk_inode.major>=N_DEV),"Invalid device file");

    // 第二步：填充file_t结构体

    file_t *file=file_alloc();
    if(!file)
        return NULL;
    
    // 这两个虽然使用不同的宏定义，但是相应的文件类型的值相同
    file->type=file_inode->disk_inode.type;

    if(file->type==FD_FILE)
    {
        file->major=file_inode->disk_inode.major;
    }
    else
    {
        file->offset=0;
    }
    file->ip=file_inode;
    inode_unlock(file_inode);

    file->readable=!!(open_mode & MODE_READ);
    file->writable=!!(open_mode & MODE_WRITE);
    return file;
}

// 释放一个file
void file_close(file_t* file)
{
    spinlock_acquire(&lk_ftable);
    assert(file->ref > 0, "file_read: file has already been closed");
    
    // 储存一些必要的字段
    int type=file->type;
    inode_t *ip=file->ip;
    int writable=file->writable;
    pipe_t *pipe=file->pipe;

    --(file->ref);
    if(file->ref==0)
    {
        file->type=FD_UNUSED;
    }
    else// 只需要减少引用后解锁
    {
        spinlock_release(&lk_ftable);
        return;
    }
    spinlock_release(&lk_ftable);

    if(type==FD_PIPE)
    {
        pipe_close(pipe, writable);
    }
    else
    {
        inode_free(ip);
    }

    return;
}

// 文件内容读取
// 返回读取到的字节数
uint32 file_read(file_t* file, uint32 len, uint64 dst, bool user)
{
    uint32 count=0;

    if(!file->readable)
        return count;
    
    switch(file->type)
    {
        case FD_FILE: // fall through
        case FD_DIR:
        inode_lock(file->ip);
        count =inode_read_data(file->ip, file->offset, len, (void *)dst, user);
        if(count!=-1)
            file_lseek(file, count, LSEEK_ADD);
        inode_unlock(file->ip);
        break;

        case FD_DEVICE:
        assert(file->major<N_DEV, "file_read: invalid device");
        count=devlist[file->major].read(len, dst, user);
        break;

        case FD_PIPE:
        count=pipe_read(file->pipe, dst, len);
        break;

        default:
        panic("file_read: unknown or unsupported file type %d",file->type);
    }
    return count;
}

// 文件内容写入
// 返回写入的字节数
uint32 file_write(file_t* file, uint32 len, uint64 src, bool user)
{
    uint32 count=0;

    if(!file->writable)
        return count;

    switch(file->type)
    {
        case FD_FILE:
        case FD_DIR:
        inode_lock(file->ip);
        count=inode_write_data(file->ip, file->offset, len, (void *)src, user);
        if(count!=-1)
            file_lseek(file, count, LSEEK_ADD);
        inode_unlock(file->ip);
        break;

        case FD_DEVICE:
        assert(file->major<N_DEV, "file_write: invalid device");
        count=devlist[file->major].write(len, src, user);
        break;

        case FD_PIPE:
        count=pipe_write(file->pipe, src, len);
        break;

        default:
        panic("file_write: unknown or unsupported file type %d",file->type);
    }
    return count;
}


// 修改file->offset (只针对FD_FILE类型的文件)
uint32 file_lseek(file_t* file, uint32 offset, int flags)
{
    assert(file->type == FD_FILE, "file_lseek: type must be FD_FILE, received file id: %d",file->type);
    switch(flags)
    {
        case LSEEK_SET:
        file->offset=offset;
        break;
        case LSEEK_ADD:
        file->offset+=offset;
        break;
        case LSEEK_SUB:
        file->offset-=offset;
        break;
        default:
        panic("file_lseek: invalid flags %d",flags);
    }
    return file->offset;
}

// file->ref++ with lock
file_t* file_dup(file_t* file)
{
    spinlock_acquire(&lk_ftable);
    assert(file->ref > 0, "file_dup: ref");
    file->ref++;
    spinlock_release(&lk_ftable);
    return file;
}

// 获取文件状态
int file_stat(file_t* file, uint64 addr)
{
    file_state_t state;
    if(file->type == FD_FILE || file->type == FD_DIR)
    {
        inode_lock(file->ip);
        state.type = file->ip->disk_inode.type;
        state.inode_num = file->ip->inode_num;
        state.nlink = file->ip->disk_inode.nlink;
        state.size = file->ip->disk_inode.size;
        inode_unlock(file->ip);

        uvm_copyout(myproc()->pgtbl, addr, (uint64)&state, sizeof(file_state_t));
    }
    return -1;
}