#ifndef __PRINT_H__
#define __PRINT_H__

#include "common.h"

void print_init(void);
void printf(const char* fmt, ...);
// void panic(char *s);
// void assert(bool condition, const char* warning);

void panic(const char *fmt, ...);
void assert(int condition, const char* warning, ...);
#endif