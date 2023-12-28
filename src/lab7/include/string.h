#pragma once

#include "types.h"

void* memset(void *, int, uint64_t);
void *memcpy(void *str1, const void *str2, size_t n);
int memcmp(const void *cs, const void *ct, size_t count);

//static inline int strcmp(const char *s1, const char *s2)
//{
//    while (*s1 && (*s1 == *s2))
//        s1++, s2++;
//    return *(const unsigned char *)s1 - *(const unsigned char *)s2;
//}
//
//static inline int strncmp(const char *s1, const char *s2, size_t n)
//{
//    long len = n;
//    while (len-- && *s1 && (*s1 == *s2))
//        s1++, s2++;
//    return len < 0 ? 0 : *(const unsigned char *)s1 - *(const unsigned char *)s2;
//}
//
//static inline char *strcpy(char *dest, const char *src)
//{
//    char *tmp = dest;
//    while ((*dest++ = *src++))
//        ;
//    return tmp;
//}
//
//static inline char *strncpy(char *dest, const char *src, size_t n)
//{
//    char *tmp = dest;
//    while (n-- && (*dest++ = *src++))
//        ;
//    return tmp;
//}

static inline int strlen(const char *str)
{
    int len = 0;
    while (*str++)
        len++;
    return len;
}