#include "syscall.h"
#include "../stdint.h"

/* 不同参数的系统调用接口 */

/* 无参数的 */
#define _syscall0(NUMBER) ({    \
    int res;                     \       
    asm volatile (               \
    "int $0x80"             \
    : "=a" (res)            \
    : "a"(NUMBER)           \
    : "memory"              \
    );            \
    res;                    \
})

#define _syscall1(NUMBER, ARG1) ({    \
    int res;                     \       
    asm volatile (               \
    "int $0x80"             \
    : "=a" (res)            \
    : "a"(NUMBER), "b"(ARG1)           \
    : "memory"              \
    );            \
    res;                    \
})

#define _syscall2(NUMBER, ARG1, ARG2) ({    \
    int res;                     \       
    asm volatile (               \
    "int $0x80"             \
    : "=a" (res)            \
    : "a"(NUMBER), "b"(ARG1), "c"(ARG2)           \
    : "memory"              \
    );            \
    res;                    \
})

#define _syscall3(NUMBER, ARG1, ARG2, ARG3) ({    \
    int res;                     \       
    asm volatile (               \
    "int $0x80"             \
    : "=a" (res)            \
    : "a"(NUMBER), "b"(ARG1), "c"(ARG2),"d"(ARG3) \
    : "memory"              \
    );            \
    res;                    \
})

/* 获取pid */
uint32_t getpid(void){
    return _syscall0(SYS_GETPID);
}