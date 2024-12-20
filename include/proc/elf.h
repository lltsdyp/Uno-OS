#ifndef __ELF_H__
#define __ELF_H__

#include "common.h"

#define ELF_MAGIC 0x464C457FU  // "\x7FELF" in little endian

/* 
    ELF文件的构成:
    elf header            全局元数据
    program header table  描述各个segments
    sections(segments)    从编译和执行两个角度
    section header table  描述各个sections
*/

typedef struct elf_header
{
    uint32 magic; // 应该是ELF_MAGIC
    uint8  elf[12]; // 一些信息
    uint16 type;  // ELF文件类型(如可执行文件、共享库等)
    uint16 machine; // 机器的指令集架构(如x86、risc-v等)
    uint32 version; // ELF版本号
    uint64 entry; // 程序入口地址
    uint64 ph_off; // program header table偏移量
    uint64 sh_off; // section header table偏移量
    uint32 flags; // 处理器特定标志
    uint16 eh_size; // elf_header本身的大小 
    uint16 ph_ent_size; // program header table里每个entry的大小 
    uint16 ph_ent_num; // program header table里的entry数量
    uint16 sh_ent_size; // section header table里每个entry的大小
    uint16 sh_ent_num; // section header table里的entry数量
    uint16 sh_str_ndx; // section header table中包含节字符串表索引的entry的索引
} elf_header_t;

// program header
typedef struct program_header
{
    uint32 type;
    uint32 flags;
    uint64 off;
    uint64 va;
    uint64 pa;
    uint64 file_size;
    uint64 mem_size;
    uint64 align;
} program_header_t;

// program_header->type = load 时载入内存
#define ELF_PROG_LOAD        1

// program_header->flags取值
#define ELF_PROG_FLAG_EXEC   1
#define ELF_PROG_FLAG_WRITE  2
#define ELF_PROG_FLAG_READ   4

// 最大参数量
#define ELF_MAXARGS          32
// 单个参数长度限制
#define ELF_MAXARG_LEN       PGSIZE

#endif