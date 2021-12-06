#ifndef __LIB_STRING_H
#define __LIB_STRING_H
#include "stdint.h"
void memset(void* dst_, uint8_t value, uint32_t size);
void memcpy(void* dst_, const void* src_, uint32_t size);
int memcmp(const void* a_, const void* b_, uint32_t size);
char* strcpy(char* dst_, const char* src_);
uint32_t strlen(const char* str);
int8_t strcmp(const char *a, const char* b);
char* strchr(const char* string, const uint8_t ch); // 字符ch在string当中最先出现的地址
char* strrchr(const char* string, const uint8_t ch);    // 同上，但从右往左
char* strcat(char* dst_, const char* src_);       // 字符串拼接
uint32_t strchrs(const char* filename, uint8_t ch);     // ch出现的次数
#endif
