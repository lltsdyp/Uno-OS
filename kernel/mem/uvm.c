#include "mem/mmap.h"
#include "mem/pmem.h"
#include "mem/vmem.h"
#include "proc/cpu.h"
#include "lib/print.h"
#include "lib/str.h"
#include "memlayout.h"

// 连续虚拟空间的复制(在uvm_copy_pgtbl中使用)
static void copy_range(pgtbl_t old, pgtbl_t new, uint64 begin, uint64 end)
{
    uint64 va, pa, page;
    int flags;
    pte_t *pte;

    for (va = begin; va < end; va += PGSIZE)
    {
        pte = vm_getpte(old, va, false);
        assert(pte != NULL, "uvm_copy_pgtbl: pte == NULL");
        assert((*pte) & PTE_V, "uvm_copy_pgtbl: pte not valid");
        // assert(PTE_CHECK(*pte), "uvm_copy_pgtbl: pte check fail");

        pa = (uint64)PTE_TO_PA(*pte);
        flags = (int)PTE_FLAGS(*pte);

        page = (uint64)pmem_alloc(false); // 传入的不可能是kernel_pgtbl吧 lol
        memcpy((char *)page, (const char *)pa, PGSIZE);
        vm_mappages(new, va, page, PGSIZE, flags);
    }
}

// 两个 mmap_region 区域合并
// 保留一个 释放一个 不操作 next 指针
// 在uvm_munmap里使用
static void mmap_merge(mmap_region_t *mmap_1, mmap_region_t *mmap_2, bool keep_mmap_1)
{
    // 确保有效和紧临
    assert(mmap_1 != NULL && mmap_2 != NULL, "mmap_merge: NULL");
    assert(mmap_1->begin + mmap_1->npages * PGSIZE == mmap_2->begin, "mmap_merge: check fail");

    // merge
    if (keep_mmap_1)
    {
        mmap_1->npages += mmap_2->npages;
        mmap_region_free(mmap_2);
    }
    else
    {
        mmap_2->begin -= mmap_1->npages * PGSIZE;
        mmap_2->npages += mmap_1->npages;
        mmap_region_free(mmap_1);
    }
}

// 打印以 mmap 为首的 mmap 链
// for debug
void uvm_show_mmaplist(mmap_region_t *mmap)
{
    mmap_region_t *tmp = mmap;
    printf("\nmmap allocable area:\n");
    if (tmp == NULL)
        printf("NULL\n");
    while (tmp != NULL)
    {
        printf("allocable region: %p ~ %p\n", tmp->begin, tmp->begin + tmp->npages * PGSIZE);
        tmp = tmp->next;
    }
}

// 递归释放 页表占用的物理页 和 页表管理的物理页
// ps: 顶级页表level = 3, level = 0 说明是页表管理的物理页
void uvm_destroy_pgtbl(pgtbl_t pgtbl,uint32 level)
{
    if(level==0)
    {
        pmem_free((uint64)pgtbl,false);
    }
    for(uint32 i=0;i<PGSIZE/sizeof(pte_t);++i)
    {
        pte_t *pte=(pte_t *)pgtbl[i];
        uvm_destroy_pgtbl((pgtbl_t)PTE_TO_PA(*pte),level-1);
    }
    pmem_free((uint64)pgtbl,true);
}

// 拷贝页表 (拷贝并不包括trapframe 和 trampoline)
void uvm_copy_pgtbl(pgtbl_t old, pgtbl_t new, uint64 heap_top, uint32 ustack_pages, mmap_region_t *mmap)
{
    /* step-1: USER_BASE ~ heap_top */
    copy_range(old, new, USER_VMEM_START, heap_top);

    /* step-2: ustack */
    copy_range(old, new, USER_STACK_BOTTOM-USER_STACK_INITIAL_PAGE_COUNT*PGSIZE, USER_STACK_BOTTOM);

    /* step-3: mmap_region */
    // 我们需要遍历mmap链，找到所有被分配掉的页面
    for(mmap_region_t *region=myproc()->mmap;region!=NULL;region=region->next)
    {
        uint64 begin=region->begin+region->npages*PGSIZE;
        uint64 end=region->next==NULL?MMAP_END:region->next->begin;
        copy_range(old,new,begin,end);
    }
}

// 在用户页表和进程mmap链里 新增mmap区域 [begin, begin + npages * PGSIZE)
// 页面权限为perm
void uvm_mmap(uint64 begin, uint32 npages, int perm)
{
    // 如果begin为0，则begin必然会在接下来被修改
    int begin_modified=!begin;
    uint64 end = begin + npages * PGSIZE;
    if (npages == 0)
        return;
    assert(begin % PGSIZE == 0, "uvm_mmap: begin not aligned");
    // 确保没有越界，begin为0的情况在后面处理
    assert(begin_modified||(begin >= MMAP_BEGIN && end<=MMAP_END),"uvm_mmap: out of range. Given range:\nbegin=%p, end=%p",begin,end);

    // 修改 mmap 链 (分情况的链式操作)
    // mmap指向的是一个特殊的region，这个region的npages设置为0（详见proc.c）
    // 之后的操作我们需要保证不会出现npages为0的region
    mmap_region_t *prev_region = myproc()->mmap;

    // 找到要进行操作的 mmap_region
    // munmap中保证所有相邻的mmap_region都已经被合并
    // 刚开始，在特殊的region后面的一个region才是真正意义上的region
    for (mmap_region_t *region = prev_region->next; region != NULL; region = region->next)
    {
        // begin是否包含在 region 中
        // 或者begin==0，且找到的当前region可以分配出npages个页面
        if ((region->begin <= begin &&
            region->begin + region->npages * PGSIZE >= begin) ||
            (begin==0 && region->npages>=npages))
        {
            // 调整begin和end区域
            if(begin==0)
            {
                begin=region->begin;
                end+=begin;
            }
            int prev_npages = (begin - region->begin) / PGSIZE; // 分配该mmap区域后，前面的那个mmap_region占据了多少页面
            // 计算分配mmap区域后，后面的那个mmap_region占据了多少页面
            int next_npages = ((region->begin + region->npages * PGSIZE) - (end)) / PGSIZE;

            assert(next_npages >= 0, "uvm_mmap: memory space insufficient:\n%d page(s) not allocated!", -next_npages);

            // 头尾均会剩下空闲页
            if (prev_npages && next_npages)
            {
                // 该情况下，需要分配一个新的mmap_region，一个管理前部一个管理后部
                mmap_region_t *new_region = mmap_region_alloc();
                region->npages = prev_npages;

                // 跳过从分配的页面的起始地址开始的npages个页面
                new_region->begin = end;
                new_region->npages = next_npages;

                // 将new_region插入链表中
                new_region->next = region->next;
                region->next = new_region;
            }
            // 只有尾部有空闲页
            else if (!prev_npages && next_npages)
            {
                // 该情况下，将当前的region的begin字段向前移动
                region->begin = end;
            }
            else if (prev_npages && !next_npages)
            {
                // 该情况下，简单的将当前region的npages字段减小即可
                region->npages = prev_npages;
            }
            else //(!prev_npages&&!next_npages)
            {
                // 该情况下，刚好将一整块region提供给用户，我们只需要简单地将该region从链表中删除
                prev_region->next = region->next;
                mmap_region_free(region);
            }

            // 修改页表 (物理页申请 + 页表映射)
            for (int i = 0; i < npages; ++i)
            {
                vm_mappages(myproc()->pgtbl, begin + i * PGSIZE, (uint64)pmem_alloc(false),
                            PGSIZE, perm);
            }

            //FOR DEBUG
            printf("mmap:\n");
            uvm_show_mmaplist(myproc()->mmap);
            // vm_print(myproc()->pgtbl);
            printf("\n");
            return;
        }
        prev_region = region;
    }

    if (begin_modified)
    {
        panic("uvm_mmap: no sequential mmap region to hold %d pages\n", npages);
    }
    // 执行到此处，说明没有找到目标的mmap页面，则报错
    // 这里个人感觉不应该能够运行到
    else
    {
        panic("uvm_mmap: region not found\nbegin:%p,npages:%d\n", begin, npages);
    }
}

// 在用户页表和进程mmap链里释放mmap区域 [begin, begin + npages * PGSIZE)
void uvm_munmap(uint64 begin, uint32 npages)
{
    uint64 end = begin + npages * PGSIZE;
    if (npages == 0)
        return;
    assert(begin % PGSIZE == 0, "uvm_munmap: begin not aligned");
    assert(begin>=MMAP_BEGIN && end<=MMAP_END,"uvm_munmap: out of range. Given range:\nbegin=%p, end=%p",begin,end);


    // new mmap_region 的产生
    mmap_region_t *new_region = mmap_region_alloc();
    new_region->begin = begin;
    new_region->npages = npages;

    // 尝试合并 mmap_region
    // 首先寻找可能的前后两个mmap_region
    mmap_region_t *prev_region = myproc()->mmap;
    mmap_region_t *next_region = prev_region->next;
    for(; next_region != NULL; next_region = next_region->next)
    {
        uint64 prev_end=prev_region->begin+prev_region->npages*PGSIZE;
        // 可以插入的条件是，在链表中，可以找到两个相邻的region，而新的region刚刚好在这两个region之间
        if(begin>=prev_end&&end<=next_region->begin)
        {
            // 前一个块与后一个相邻，且前一个块并不是用于标识链表头的特殊块，则可以尝试合并
            if(begin==prev_end&&prev_region->npages!=0)
            {
                mmap_merge(prev_region,new_region,true);
                // 同时检测一下下一个块是否可以同时合并掉
                if(end==next_region->begin)
                {
                    prev_region->next=next_region->next;
                    mmap_merge(prev_region,next_region,true);
                }
            }
            // 前一个区域不与new_region相邻但是后一个相邻
            else if(end==next_region->begin)
            {
                mmap_merge(new_region,next_region,false);
            }
            else //均不相邻，将其插入即可
            {
                prev_region->next=new_region;
                new_region->next = next_region;
            }
            break;
        }
        prev_region=next_region;
    }

    // 如果找到这里还没找到应当插入的位置，那么一定是放在链表末端（我们前面已经保证给定的mmap区域不越界）
    if(next_region==NULL)
    {
        prev_region->next = new_region;
        uint64 prev_end = prev_region->begin + prev_region->npages * PGSIZE;
        if (begin == prev_end)
        {
            mmap_merge(prev_region, new_region, true);
        }
    }

    // 页表释放
    vm_unmappages(myproc()->pgtbl, begin, npages*PGSIZE, true);
    //FOR DEBUG
    printf("vm_munmap");
    uvm_show_mmaplist(myproc()->mmap);
    // vm_print(myproc()->pgtbl);
    printf("\n");
}

// 用户堆空间增加, 返回新的堆顶地址 (注意栈顶最大值限制)
// 在这里无需修正 p->heap_top
uint64 uvm_heap_grow(pgtbl_t pgtbl, uint64 heap_top, uint32 len)
{
    uint64 new_heap_top = heap_top + len;
    uint64 ptr;
    void *pg;

    for (ptr = heap_top; ptr < new_heap_top; ptr += PGSIZE)
    {
        pg = pmem_alloc(false);

        // assert(pg == NULL, "uvm_heap_grow failed");

        vm_mappages(pgtbl, ptr, (uint64)pg, PGSIZE, PTE_W | PTE_R | PTE_U);
        memset(pg, 0, PGSIZE);
    }

    return new_heap_top;
}

// 用户堆空间减少, 返回新的堆顶地址
// 在这里无需修正 p->heap_top
uint64 uvm_heap_ungrow(pgtbl_t pgtbl, uint64 heap_top, uint32 len)
{
    uint64 new_heap_top = heap_top - len;

    // 将新的堆顶向下对齐到页面边界
    uint64 aligned_new_heap_top = PGROUNDUP(new_heap_top);
    uint64 ptr = PGROUNDUP(heap_top);

    // // 在减少堆空间时，new_heap_top 可能会低于堆的最低起始地址，避免错误地释放不属于堆的页面
    // assert(new_heap_top >= USER_VMEM_START, "uvm_heap_ungrow: new heap top out of range");

    // 遍历从当前堆顶到新的堆顶之间的所有页
    while (ptr > aligned_new_heap_top)
    {
        ptr -= PGSIZE;
        vm_unmappages(pgtbl, ptr, PGSIZE, true); // 解除映射并释放物理页
    }

    return new_heap_top;
}

// 用户态地址空间[src, src+len) 拷贝至 内核态地址空间[dst, dst+len)
// 注意: src dst 不一定是 page-aligned
void uvm_copyin(pgtbl_t pgtbl, uint64 dst, uint64 src, uint32 len)
{
    // 找到src对应页的开头地址
    uint64 dstbeg = dst;
    for (uint64 srcbeg = PGROUNDDOWN(src); srcbeg < src + len; srcbeg += PGSIZE)
    {
        pte_t *pte = vm_getpte(pgtbl, srcbeg, 0);
        assert(pte != NULL && (*pte & PTE_V), "uvm_copyin: can't find page contains address %p", srcbeg);
        // 页表项对应的物理地址
        uint64 paddr = PTE_TO_PA(*pte);

        // 只会出现在第一页上，即，页面开头地址小于需要复制的位置的地址
        if (srcbeg <= src)
        {
            // src&0xfff即src%PGSIZE(0xfff)
            // 同时，我们需要小心len过小导致复制不到一个页面尾部的情况
            uint32 n = PGSIZE - (src & 0xfff) < len ? PGSIZE - (src & 0xfff) : len;
            memcpy((void *)dstbeg, (void *)(paddr + (src & 0xfff)), n);
            dstbeg += n;
        }
        // 只会出现在最后一页上，即，该页尾部不是需要复制的内容
        else if (paddr + PGSIZE >= src + len)
        {
            uint32 n = src & 0xfff;
            memcpy((void *)dstbeg, (void *)paddr, n);
            dstbeg += n;
        }
        else
        {
            memcpy((void *)dstbeg, (void *)paddr, PGSIZE);
            dstbeg += PGSIZE;
        }
    }
}

// 内核态地址空间[src, src+len） 拷贝至 用户态地址空间[dst, dst+len)
void uvm_copyout(pgtbl_t pgtbl, uint64 dst, uint64 src, uint32 len)
{
    uint64 srcbeg = src;
    for (uint64 dstbeg = PGROUNDDOWN(dst); dstbeg < dst + len; dstbeg += PGSIZE)
    {
        pte_t *pte = vm_getpte(pgtbl, dstbeg, 0);
        assert(pte != NULL && (*pte & PTE_V), "uvm_copyout: can't find page contains address %p", dstbeg);
        uint64 paddr = PTE_TO_PA(*pte);

        if (dstbeg <= dst)
        {
            // 同uvm_copyin
            uint32 n = PGSIZE - (dst & 0xfff) < len ? PGSIZE - (dst & 0xfff) : len;
            memcpy((void *)(paddr + (dst & 0xfff)), (void *)srcbeg, n);
            srcbeg += n;
        }
        else if (paddr + PGSIZE >= dst + len)
        {
            uint32 n = dst & 0xfff;
            memcpy((void *)paddr, (void *)srcbeg, n);
            srcbeg += n;
        }
        else
        {
            memcpy((void *)paddr, (void *)srcbeg, PGSIZE);
            srcbeg += PGSIZE;
        }
    }
}

// 用户态字符串拷贝到内核态
// 最多拷贝maxlen字节, 中途遇到'\0'则终止
// 注意: src dst 不一定是 page-aligned
void uvm_copyin_str(pgtbl_t pgtbl, uint64 dst, uint64 src, uint32 maxlen)
{
    uint64 dstbeg = dst, offset = src & 0xfff, srcbeg = PGROUNDDOWN(src);
    uint64 paddr = 0;
    for (uint32 count = 0; count < maxlen; ++count)
    {
        // 由于offset每次循环自增1，当offset%PGSIZE==0时，说明遇到了新的页面
        if (paddr == 0 || (offset & 0xfff) == 0)
        {
            pte_t *pte = vm_getpte(pgtbl, srcbeg, 0);
            assert(pte != NULL && (*pte & PTE_V), "uvm_copyin_str: can't find page contains address %p", dstbeg);
            paddr = PTE_TO_PA(*pte);
            srcbeg += PGSIZE; // 下次直接访问下一页
        }
        char c = *(char *)(paddr + offset);
        if (c != '\0')
        {
            *(char *)dstbeg = c;
            ++offset;
            ++dstbeg;
        }
        else
        {
            break;
        }
    }
}