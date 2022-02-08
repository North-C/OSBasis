#include "timer.h"
#include "io.h"
#include "print.h"
#include "thread.h"
#include "interrupt.h"
#include "debug.h"

#define IRQ0_FREQUENCY    100
#define INPUT_FREQUENCY   1193180
#define COUNTER0_VALUE      INPUT_FREQUENCY / IRQ0_FREQUENCY
#define COUNTER0_PORT        0x40
#define COUNTER0_NO         0
#define COUNTER0_MODE       2         
#define READ_WRITE_LATCH    3
#define PIT_CONTROL_PORT    0x43

// 注册时钟中断函数
uint32_t ticks;         // ticks 是内核自中断开启以来总共的滴答数



/* 计数器的counter_no, 读写锁属性rwl, 计数器模式counter_mode写入模式控制寄存器并赋初始值counter_value*/
static void frequency_set(uint8_t counter_port, uint8_t counter_no, uint8_t rwl,
                            uint8_t counter_mode, uint16_t counter_value){
    // 向控制端口0x43写入控制字
    outb(PIT_CONTROL_PORT, (uint8_t)(counter_no << 6 | rwl << 4 | counter_mode << 1));
    // 写入counter_value的低8位
    outb(counter_port, (uint8_t) counter_value);
    outb(counter_port, (uint8_t)(counter_value >> 8));
}   

/* 时钟中断处理函数 */
static void intr_timer_handler(void){
    struct task_struct* cur_thread = running_thread();
    
    // put_str("cur_therad's name is: ");
    // put_str(cur_thread->name);
    // put_char('\n');

    ASSERT(cur_thread->stack_magic == 0x19870916);

    ticks++;
    cur_thread->elapsed_ticks++;

    if(cur_thread->ticks == 0){
        schedule();     // 没有剩余时间片，则进行调度
    }else{
        cur_thread->ticks--;                // 递减时间片
    }
}

/* 初始化PIT8253 */
void timer_init(){
    put_str("timer_init start\n");
    frequency_set(COUNTER0_PORT, COUNTER0_NO, READ_WRITE_LATCH, 
                    COUNTER0_MODE, COUNTER0_VALUE);
    register_handler(0x20, intr_timer_handler);             // 注册时钟中断函数

    put_str("timer_init done\n");
}





