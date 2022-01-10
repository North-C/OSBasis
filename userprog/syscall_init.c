#include "syscall_init.h"
#include "../stdint.h"
#include "sync.h"
#include "../lib/user/syscall.h"
#include "../thread/thread.h"
#include "../lib/kernel/print.h"

#define syscall_nr 32
typedef void* syscall;     // 指针，指向函数入口
syscall syscall_table[syscall_nr];

uint32_t sys_getpid(void){  // 为什么返回的是uint32_t ，而不是int16_t????
    return running_thread()->pid;
}

/* 初始化系统调用 */
void syscall_init(void){
    put_str("syscall_init start\n");
    syscall_table[SYS_GETPID] = sys_getpid;
     put_str("syscall_init done\n");
}