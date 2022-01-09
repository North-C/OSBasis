#ifndef __LIB_KERNEL_SYSCALL_H
#define __LIB_KERNEL_SYSCALL_H
#include "../stdint.h"
enum SYSCALL_NR {
    SYS_GETPID
};
uint32_t getpid(void);
#endif
