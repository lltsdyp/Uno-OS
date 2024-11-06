#ifndef __INTRNO_H__
#define __INTRNO_H__

// 判断中断类型时不要直接使用magic number
// 而是在这里新建一个常量来说明其意义

#define SMODE_SOFTWARE_INTERRUPT 0x01
#define SMODE_EXTERNAL_INTERRUPT 0x09
#define UMODE_SYSCALL_INTERRUPT  0x08
#define UMODE_INSTRUCTION_PAGE_FAULT 0x0c

#endif
