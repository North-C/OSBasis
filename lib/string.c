#include "string.h"
#include "debug.h"
#include "global.h"

/* 将dst_ 起始的size 个字节置为value */
void memset(void* dst_, uint8_t value, uint32_t size){
    ASSERT(dst_ != NULL);
    uint8_t* dst = (uint8_t*)dst_;

    while(size-- >0)
        *dst++ = value;       // ++ 和 *都是右结合的
}

/* 将src_当中起始的size个字节复制到 dst_当中*/
void memcpy(void* dst_, const void* src_, uint32_t size){
    ASSERT(dst_ != NULL && src_ != NULL);
    uint8_t* dst = dst_;
    const uint8_t* src = src_;      // 常量在定义时初始化

    while( size-- > 0){
         *dst++ = *src++;
    }
}

/* 连续比较a_ 和 b_的前size个字节,相等则返回0, a_大则返回1, b_大返回-1 */
int memcmp(const void* a_, const void* b_, uint32_t size){
    const char* a = a_;
    const char* b = b_;
    ASSERT(a != NULL && b != NULL);
    while(size-- > 0){
        if(*a != *b) {
            return *a > *b ? 1 : -1;
        }
        a++; b++;
    }
    return 0;
}
/* 将字符串src_ 复制到 dst_当中 */
char* strcpy(char* dst_, const char* src_){
    ASSERT(src_ != NULL && dst_ != NULL);
    char* dst = dst_;
    while((*dst_++ = *src_++)) ;       

    return dst;
}

/* 计算字符串str的长度 */
uint32_t strlen(const char* str){
    ASSERT(str != NULL);
    const char* p = str;        // 不能丢失const属性!!!!!!
    while(*p++);
    return (p - str - 1);
}

/* 比较字符串a 和字符串b，如果a大，返回1；b大，返回-1；相等则返回0 */
int8_t strcmp(const char *a, const char* b){
    ASSERT(a != NULL && b != NULL);
    while((*a != 0) && (*a == *b)){
        a++;
        b++;
    }
    return *a > *b ? 1: *a < *b ;
}
/* 从左至右，查找字符ch在字符串string当中最先出现的地址 */
char* strchr(const char* string, const uint8_t ch){
    ASSERT(string != NULL);
    while(*string != 0){
        if(*string == ch){
            return (char*)string;
        }
        string++;
    }
    return NULL;
}

/* 从右至左，查找字符ch在字符串string 当中最先出现的位置 */
char* strrchr(const char* string, const uint8_t ch){
    ASSERT(string != NULL);
    const char *last_char = NULL;
    while(*string != 0){
        if(*string == ch){
            last_char = string;
        }
        string++;
    }
    return (char*)last_char;    // 进行强制类型转换,消去const的属性
}

/* 将字符串 dst_和 src_ 拼接到一起, 返回拼接后的地址 */
char* strcat(char* dst_, const char* src_){
    ASSERT(dst_ != NULL && src_ != NULL);
    char *str = dst_;
    while(*str++);
    --str;
    while((*str++ = *src_++ ));    
    return dst_;
}

/* 计算 字符ch 在filename字符串当中出现的次数 */
uint32_t strchrs(const char* filename, uint8_t ch){
    ASSERT(filename != NULL);
    uint32_t count = 0;
    while(*filename != 0){
        if(*filename == ch){
            count++;
        }
        filename++;
    }
    return count;
}

