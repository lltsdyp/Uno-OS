#include "proc/cpu.h"
#include "proc/elf.h"
#include "mem/vmem.h"
#include "mem/pmem.h"
#include "mem/mmap.h"
#include "fs/inode.h"
#include "fs/dir.h"
#include "lib/print.h"
#include "lib/str.h"
#include "memlayout.h"

// 对于ip管理的ELF文件
// 将[offset, offset + size)的磁盘区域 (一个segment)
// 映射到pgtbl里的 [va, va + size)区域
// 成功返回0 失败返回-1
static int load_segment(inode_t* ip, uint32 offset, pgtbl_t pgtbl, uint64 va, uint32 size)
{
    assert(va % PGSIZE == 0, "load_segment: va must be page aligned");

    pte_t* pte;
    uint64 pa;
    uint32 read_len;
    for(uint32 off = 0; off < size; off += PGSIZE)
    {
        pte = vm_getpte(pgtbl, va + off, false);
        pa = PTE_TO_PA(*pte);
        assert(pa != 0, "load_segment: addr should exist");
        read_len = (size - off < PGSIZE) ? size - off : PGSIZE;
        if(inode_read_data(ip, offset, read_len, (void*)pa, false) != read_len)
            return -1;
    }
    return 0;
}

// 执行一个ELF文件
// 给出文件路径和参数
// 成功返回argc 失败返回-1
int proc_exec(char* path, char** argv)
{
    // 获取当前进程并记录一些地址空间信息
    proc_t* p = myproc();
    pgtbl_t old_pgtbl = p->pgtbl;
    mmap_region_t* old_mmap = p->mmap;

    // 建立新进程的页表
    pgtbl_t pgtbl = proc_pgtbl_init((uint64)(p->tf));

//------------------------------- 静态数据区 + 代码区 ----------------------------------

    // 获取ELF文件对应的inode
    inode_t* ip = path_to_inode(path);
    if(ip == NULL) return -1;
    inode_lock(ip);


    // 读取elf_header并检查魔数
    elf_header_t eh;
    if(inode_read_data(ip, 0, sizeof(eh), &eh, false) != sizeof(eh))
        goto bad;
    if(eh.magic != ELF_MAGIC)
        goto bad;

    // 根据program header table载入ELF文件的各个segment
    // 讲各个segment填充到用户的地址空间
    program_header_t ph;
    uint64 new_heap_top = 0, heap_top = USER_BASE;
    for(uint32 off = eh.ph_off; off < eh.ph_off + eh.ph_ent_num * sizeof(ph); off += sizeof(ph))
    {
        // 读取一个program header
        if(inode_read_data(ip, off, sizeof(ph), &ph, false) != sizeof(ph))
            goto bad;

        // 只处理需要被load进内存的
        if(ph.type != ELF_PROG_LOAD) 
            continue;
        
        // 一些检查
        if(ph.mem_size < ph.file_size)
            goto bad;
        if(ph.va + ph.mem_size < ph.va)
            goto bad;
        if(ph.va % PGSIZE != 0)
            goto bad;

        // 申请新的物理页并映射到页表
        new_heap_top = uvm_heap_grow(pgtbl, heap_top, ph.va + ph.mem_size - heap_top, PTE_R | PTE_X);
        if(new_heap_top != ph.va + ph.mem_size)
            goto bad;
        heap_top = new_heap_top;

        // 加载文件里的segment到对应位置
        if(load_segment(ip, ph.off, pgtbl, ph.va, ph.file_size) < 0)
            goto bad;
    }
    // 解锁和释放inode
    inode_unlock_free(ip);
    ip = NULL;

    // 设置heap_top
    p->heap_top = ALIGN_UP(heap_top, PGSIZE);

// ------------------------------------ 用户栈 -----------------------------------------------

    // 准备ustack
    uint64 page = (uint64)pmem_alloc(false);
    uint64 sp = TRAPFRAME;
    vm_mappages(pgtbl, sp - PGSIZE, page, PGSIZE, PTE_R | PTE_W | PTE_U);
    p->ustack_pages = 1;

    // 向ustack里面填充参数 + sp_list
    uint64 sp_list[ELF_MAXARGS + 1];
    uint32 argc = 0, arg_len = 0;
    for(argc = 0; argv[argc]; argc++) 
    {
        // 检查参数量
        if(argc >= ELF_MAXARGS)
            goto bad;

        // 准备参数空间
        arg_len = strlen(argv[argc]) + 1;
        sp -= arg_len;
        sp -= sp % 16; // risc-v要求参数必须是16byte-aligned
        
        // 参数空间越界
        if(sp < TRAPFRAME - PGSIZE)
            goto bad;
        
        // 在用户地址空间填写参数
        uvm_copyout(pgtbl, sp, (uint64)argv[argc], arg_len);

        // 设置sp_list
        sp_list[argc] = sp;
    }
    sp_list[argc] = 0;

    arg_len = (argc + 1) * sizeof(uint64); 
    sp -= arg_len;
    sp -= sp % 16;
    if(sp < TRAPFRAME - PGSIZE)
        goto bad;
    uvm_copyout(pgtbl, sp, (uint64)sp_list, arg_len);

    // int main(int argc, char* argv[])
    p->tf->a1 = sp;

// --------------------- 旧地址空间的销毁 + 新地址空间的设置 ----------------------------

    p->tf->epc = eh.entry; // pc
    p->tf->sp = sp;        // sp

    // 释放用户地址空间 + 释放占用的mmap结构体
    uvm_destroy_pgtbl(old_pgtbl);
    mmap_region_t *tmp = old_mmap, *tmp_next;
    while (tmp)
    {
        tmp_next = tmp->next;
        mmap_region_free(tmp);
        tmp = tmp_next;
    }

    // 设置新的地址空间 + 申请mmap结构体
    p->pgtbl = pgtbl;
    p->mmap = mmap_region_alloc(true);

// ------------------------- 正常返回 or 异常返回 --------------------
    return argc;

bad:
    if(pgtbl)
        uvm_destroy_pgtbl(pgtbl);
    if(ip)
        inode_unlock_free(ip);
    panic("exec fail");
    return -1;
}
