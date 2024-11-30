#ifndef __PRINT_H__
#define __PRINT_H__

#include "common.h"
#include <stdarg.h>

void print_init(void);
void printf(const char* fmt, ...);
// void panic(char *s);
// void assert(bool condition, const char* warning);

void vpanic(const char *fmt, va_list ap);
void panic(const char *fmt, ...);
void assert(uint64 condition, const char* warning, ...);
#endif