
#ifndef __LIB_KERNEL_SYSCALL_INIT_H
#define __LIB_KERNEL_SYSCALL_INIT_H
#include "stdint.h"
uint32_t sys_getpid();
void syscall_init(void);
#endif

