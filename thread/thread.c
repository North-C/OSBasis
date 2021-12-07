#include "thread.h"
#include "string.h"
#include "memory.h"
#include "stdint.h"
#include "global.h"

#define PG_SIZE 4096

/* 通过kernel_thread去执行 function函数 */
static void kernel_thread(thread_func* function, void* func_arg ){
    function(func_arg);
}

/* 创建线程， 将要执行的function(func_arg)放到pthread对应位置 */
void thread_create(struct task_struct* pthread, thread_func function, void* func_arg){  
    // 预留中断所用的栈的空间
    pthread->self_kstack -= sizeof(struct intr_stack);

    // 留出线程栈的空间
    pthread->self_kstack -= sizeof(struct thread_stack);        // 栈设置到地址最低端，即栈顶
    // 定义线程栈的内容
    struct thread_stack* kthread = (struct thread_stack*)pthread->self_kstack;
    kthread->eip = kernel_thread;               // 设置要执行的函数
    kthread->function = function;       
    kthread->func_arg = func_arg;
    kthread->ebp = kthread->ebx = kthread->edi = kthread->esi = 0;      // 将这几个寄存器初始化为0
}

/* 初始化线程PCB的名字name和优先级priority */
void init_thread(struct task_struct* pthread, char* name, int priority){
    memset( pthread ,0, sizeof(*pthread));       // 逐字节清零, 计算的是 *pthread 结构体的大小
    strcpy(pthread->name, name);        // 复制名字
    pthread->status = TASK_RUNNING;         // 设置为运行状态
    pthread->priority = priority;
    // self_kstack是栈顶地址,PCB占据1页的大小
    pthread->self_kstack = (uint32_t*)((uint32_t)pthread + PG_SIZE);    // pthread地址做一下uint32_t类型转换
    pthread->stack_magic = 0x19870916;      // 自定义的魔数
}

/* 创建名字为name,优先级为priority的，执行function(func_args)的线程，成功则返回对应PCB */
struct task_struct* thread_start(char* name, int priority, thread_func function, void* func_args){
    // 定义pthread, 在内核中分配1页内存
    struct task_struct* pthread = get_kernel_pages(1);      
    
    init_thread(pthread, name, priority);           // 初始化其他部分为0
    thread_create(pthread, function, func_args);        // 将self_kstack放到栈的栈顶，地址最低端
// 将栈顶指针self_kstack 放入esp , 线程栈当中用到的管理寄存器，弹出栈保存起来， ret将栈顶的数据返回到EIP当中
    asm volatile("movl %0, %%esp; pop %%ebp; pop %%ebx; pop %%edi; pop %%esi; ret": :"g" (pthread->self_kstack) : "memory");    

    return pthread;
}

