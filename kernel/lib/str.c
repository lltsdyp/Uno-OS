#include "lib/str.h"

#pragma GCC optimize ("O0")

// 从begin开始对连续n个字节赋值data
void memset(void* begin, uint8 data, uint32 n)
{
    uint8* L = (uint8*)begin;
    for (uint32 i = 0; i < n; i++)
        L[i] = data;
}

// copy
void memcpy(void *dst, const void* src, uint32 n)
{
    const char *s = src;
    char *d = dst;
    while(n) {
        *d = *s;
        n--;
        d++;
        s++;
    }
}

// 字符串p的前n个字符与q做比较
// 按照ASCII码大小逐个比较
// 相同返回0 大于或小于返回正数或负数
int strncmp(const char *p, const char *q, uint32 n)
{
    while(n > 0 && *p && *p == *q)
      n--, p++, q++;
    if(n == 0)
      return 0;
    return (uint8)*p - (uint8)*q;
}

uint64 strlen(const char *s)
{
    uint64 i = 0;
    while(s[i] != '\0')
        i++;
    return i;
}

