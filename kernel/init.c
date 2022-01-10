#include "init.h"
#include "print.h"
#include "interrupt.h"
#include "../device/timer.h"
#include "thread.h"
#include "keyboard.h"
#include "console.h"
#include "memory.h"
#include "../userprog/tss.h"
#include "../userprog/syscall_init.h"

void init_all(){
    put_str("init all\n");
    idt_init(); // 初始化并加载IDT
    mem_init();     // 初始化内存管理系统
    thread_init();
    timer_init();      // 初始化PIT
    console_init();
    keyboard_init();
    tss_init();     // tss初始化
    syscall_init();     // 系统调用初始化
}
