#ifndef __STR_H__
#define __STR_H__

#include "common.h"

void   memset(void* begin, uint8 data, uint32 n);
void   memmove(void* dst, const void* src, uint32 n);
int    strncmp(const char *p, const char *q, uint32 n);

#endif