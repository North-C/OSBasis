#include "syscall_init.h"
#include "../stdint.h"
#include "sync.h"
#include "../lib/user/syscall.h"
#include "../thread/thread.h"
#include "../lib/kernel/print.h"
#include "console.h"
#include "../lib/string.h"
#include "../kernel/memory.h"


#define syscall_nr 32
typedef void* syscall;     // 指针，指向函数入口
syscall syscall_table[syscall_nr];
/* 返回当前进程的pid */
uint32_t sys_getpid(void){  // 为什么返回的是uint32_t ，而不是int16_t????
    return running_thread()->pid;
}

/* 输出str到屏幕 */
uint32_t sys_write(char *str){  // 为什么返回的是uint32_t ，而不是int16_t????
    console_put_str(str);
    return strlen(str);
}

/* 初始化系统调用 */
void syscall_init(void){
    put_str("syscall_init start\n");
    syscall_table[SYS_GETPID] = sys_getpid;
    syscall_table[SYS_WRITE] = sys_write;
    syscall_table[SYS_MALLOC] = sys_malloc;
    syscall_table[SYS_FREE] = sys_free;
    put_str("syscall_init done\n");
}