#include "init.h"
#include "print.h"
#include "interrupt.h"
#include "../device/timer.h"

void init_all(){
    put_str("Start init\n");
    idt_init(); // 初始化并加载IDT
    timer_init();      // 初始化PIT
}
