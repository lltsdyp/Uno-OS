// kernel virtual memory management

#include "mem/pmem.h"
#include "mem/vmem.h"
#include "lib/print.h"
#include "lib/str.h"
#include "riscv.h"
#include "memlayout.h"

static pgtbl_t kernel_pgtbl; // 内核页表

static inline void change_pagetable(pgtbl_t pgtbl)
{
  w_satp(MAKE_SATP(pgtbl));
  sfence_vma(); //每次切换页表之后都需要刷新TLB表项
}

// 根据pagetable,找到va对应的pte
// 若设置alloc=true 则在PTE无效时尝试申请一个物理页
// 成功返回PTE, 失败返回NULL
// 提示：使用 VA_TO_VPN PTE_TO_PA PA_TO_PTE
pte_t *vm_getpte(pgtbl_t pgtbl, uint64 va, bool alloc)
{
    // 虚拟地址不能大于VA_MAX
    assert(va<=VA_MAX,"vm_getpte: invalid virtual address:%x",va);

    uint64 round_va=PGROUNDDOWN(va);
    pte_t *pte=NULL;
    pgtbl_t result=pgtbl;

    for(int i=PGTABLE_TOPLEVEL;i>0;--i)
    {
        int idx=VA_TO_VPN(round_va,i);
        pte = &result[idx];

        // 此前该虚拟页已被分配
        if(*pte & PTE_V)
        {
            result=(pgtbl_t)PTE_TO_PA(*pte);
        }
        else // 否则
        {
            // 不需要分配或物理内存不足则返回NULL
            if(!alloc || (result=(pgtbl_t)pmem_alloc(false))==NULL)
            {
                return ((pgtbl_t)NULL);
            }
            // 分配页面
            memset(result,0,PGSIZE);
            *pte=PA_TO_PTE(*pte)|PTE_V;
        }
    }

    return (pte_t*)result[VA_TO_VPN(va,0)];
}

// 在pgtbl中建立 [va, va + len) -> [pa, pa + len) 的映射
// 本质是找到va在页表对应位置的pte并修改它
// 检查: va pa 应当是 page-aligned, len(字节数) > 0, va + len <= VA_MAX
// 注意: perm 应该如何使用
void vm_mappages(pgtbl_t pgtbl, uint64 va, uint64 pa, uint64 len, int perm)
{
    assert(va % PGSIZE == 0 && pa % PGSIZE == 0,
           "vm_mappages: page alignment required.\n\t va: %x pa %x", va, pa);
    assert(len > 0, "vm_mappages: length should > 0\n");
    assert(va + len -1< VA_MAX, "vm_mappages: va + len -1= %x >= VA_MAX\n\t", va + len-1);
    
    // 不包含va+len
    uint64 end=PGROUNDDOWN(va+len-1);
    uint64 dst=pa;
    
    for(uint64 beg=va;beg<=end;beg+=PGSIZE)
    {
        pte_t *pte=vm_getpte(pgtbl,beg,1);
        assert(pte!=NULL,"vm_mappages: cannot find pte for va:%x",beg);
        assert(!(*pte&PTE_V),"vm_mappages: remap at %x",PTE_TO_PA(dst));
        *pte=PA_TO_PTE(dst)|perm|PTE_V;
        dst+=PGSIZE;
    }
}

// 解除pgtbl中[va, va+len)区域的映射
// 如果freeit == true则释放对应物理页, 默认是用户的物理页
void vm_unmappages(pgtbl_t pgtbl, uint64 va, uint64 len, bool freeit)
{
    assert(va % PGSIZE == 0, "vm_unmappages: page alignment required.\n\t va: %x", va);
    assert(len > 0, "vm_unmappages: length should > 0\n");
    assert(va + len-1 < VA_MAX, "vm_unmappages: va + len = %x >= VA_MAX\n\t", va + len-1);

    uint64 end=PGROUNDDOWN(va+len-1);
    for(uint64 beg=va;beg<=end;beg+=PGSIZE)
    {
        pte_t *pte=vm_getpte(pgtbl,beg,0);
        assert(pte!=NULL,"vm_unmappages: cannot find pte for va:%x",beg);
        assert(*pte&PTE_V,"vm_unmappages: %x already unmapped.",beg);
        assert(PTE_FLAGS(*pte)!=PTE_V,"vm_unmappages: %x is NOT a leaf page.",PTE_TO_PA(*pte));
        if(freeit)
        {
            pmem_free(PTE_TO_PA(*pte),1);
        }
        *pte=0;
    }
}

// 完成 UART CLINT PLIC 内核代码区 内核数据区 可分配区域 的映射
// 相当于填充kernel_pgtbl
void kvm_init()
{
    kernel_pgtbl=(pgtbl_t)pmem_alloc(true);
    assert(kernel_pgtbl!=NULL,"kvm_init: failed to initialize kernel page table.");


    vm_mappages(kernel_pgtbl, UART_BASE, UART_BASE, 
            UART_REGION_SIZE, PTE_R | PTE_W);
    vm_mappages(kernel_pgtbl, CLINT_BASE, CLINT_BASE,
            CLINT_REGION_SIZE, PTE_R | PTE_W);
    vm_mappages(kernel_pgtbl, PLIC_BASE, PLIC_BASE,
            PLIC_REGION_SIZE, PTE_R|PTE_W);

    // kernel
    vm_mappages(kernel_pgtbl, (uint64)KERNEL_BASE, (uint64)KERNEL_BASE,
            (uint64)KERNEL_DATA-(uint64)KERNEL_BASE,PTE_R|PTE_W|PTE_X);
    vm_mappages(kernel_pgtbl, (uint64)KERNEL_DATA, (uint64)KERNEL_DATA,
            (uint64)ALLOC_BEGIN-(uint64)KERNEL_DATA,PTE_R|PTE_W);
    vm_mappages(kernel_pgtbl,(uint64)ALLOC_BEGIN,(uint64)ALLOC_BEGIN,
            (uint64)ALLOC_END-(uint64)ALLOC_BEGIN,PTE_R|PTE_W);
}

// 使用新的页表，刷新TLB
void kvm_inithart()
{
    change_pagetable(kernel_pgtbl);
}

// for debug
// 输出页表内容
void vm_print(pgtbl_t pgtbl)
{
    // 顶级页表，次级页表，低级页表
    pgtbl_t pgtbl_2 = pgtbl, pgtbl_1 = NULL, pgtbl_0 = NULL;
    pte_t pte;

    printf("level-2 pgtbl: pa = %p\n", pgtbl_2);
    for (int i = 0; i < PGSIZE / sizeof(pte_t); i++)
    {
        pte = pgtbl_2[i];
        if (!((pte)&PTE_V))
            continue;
        assert(PTE_CHECK(pte), "vm_print: pte check fail (1)");
        pgtbl_1 = (pgtbl_t)PTE_TO_PA(pte);
        printf(".. level-1 pgtbl %d: pa = %p\n", i, pgtbl_1);

        for (int j = 0; j < PGSIZE / sizeof(pte_t); j++)
        {
            pte = pgtbl_1[j];
            if (!((pte)&PTE_V))
                continue;
            assert(PTE_CHECK(pte), "vm_print: pte check fail (2)");
            pgtbl_0 = (pgtbl_t)PTE_TO_PA(pte);
            printf(".. .. level-0 pgtbl %d: pa = %p\n", j, pgtbl_2);

            for (int k = 0; k < PGSIZE / sizeof(pte_t); k++)
            {
                pte = pgtbl_0[k];
                if (!((pte)&PTE_V))
                    continue;
                assert(!PTE_CHECK(pte), "vm_print: pte check fail (3)");
                printf(".. .. .. physical page %d: pa = %p flags = %d\n", k, (uint64)PTE_TO_PA(pte), (int)PTE_FLAGS(pte));
            }
        }
    }
}