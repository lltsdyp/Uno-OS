#ifndef __PMEM_H__
#define __PMEM_H__

#include "common.h"

// 来自kernel.ld
extern char KERNEL_DATA[];
extern char ALLOC_BEGIN[];
extern char ALLOC_END[];

void  pmem_init(void);
void* pmem_alloc(bool in_kernel);
void  pmem_free(uint64 page, bool in_kernel);
void  freeRange(uint64 begin, uint64 end, bool in_kernel);

// 分别用于将给定的字节数向上舍入到最接近的内存页大小的倍数
// 以及向下舍入到最接近的内存页边界
#define PGROUNDUP(sz)  ((uint64)(((sz)+PGSIZE-1) & ~(PGSIZE-1)))
#define PGROUNDDOWN(a) ((uint64)(((a)) & ~(PGSIZE-1)))

#endif