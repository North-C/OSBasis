#include "thread.h"
#include "string.h"
#include "memory.h"
#include "stdint.h"
#include "global.h"
#include "interrupt.h"
#include "../lib/kernel/list.h"
#include "debug.h"

#define PG_SIZE 4096



struct task_struct* main_thread;    // 主线程PCB
struct list thread_ready_list;      // 线程就绪队列
struct list thread_all_list;        // 所有任务队列
static struct list_elem* thread_tag;    // 队列当中的线程节点


// 执行线程切换
extern void switch_to(struct task_struct* cur, struct task_struct* old);

/* 进行任务调度 */
void schedule(void){
    ASSERT(get_intr_status() == INT_OFF);     // 需要打开中断

    struct task_struct* cur_thread = running_thread();
    if(cur_thread->status == TASK_RUNNING){     // 运行中
        // 时间片到了
        ASSERT(!list_search(&thread_ready_list, &cur_thread->general_tag));
        list_append(&thread_ready_list, &cur_thread->general_tag);
        cur_thread->ticks = cur_thread->priority;       // 重置为优先级，不至于被立即换下
        cur_thread->status = TASK_READY;
    }else{
        // 另一种情况就是被阻塞，这时不需要处理就绪队列，因为不处于就绪状态，不在该队列上
    }
    // 选一个任务来执行
    ASSERT(!list_empty(&thread_ready_list));       
    thread_tag = NULL;      // thread_tag 清空
    thread_tag = list_pop(&thread_ready_list);
    struct task_struct* task = elem2entry(thread_tag, struct task_struct, general_tag);
    task->status = TASK_RUNNING;
    switch_to(cur_thread, task);
}

/* 获取当前正在运行线程的PCB指针 */
struct task_struct* running_thread(){
    uint32_t esp;       // PCB有4KB大小，获取esp的前20位即可

    asm volatile ("mov %%esp, %0":"=g"(esp));
    return (struct task_struct*)(esp &0xfffff000);
}

/* 通过kernel_thread去执行 function函数 */
static void kernel_thread(thread_func* function, void* func_arg ){
    enable_intr();      // function运行之前要打开中断,防止function独占处理器
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
    pthread->priority = priority;

    if(pthread == main_thread){     // 依据是否是主线程来进行判断
        pthread->status = TASK_RUNNING;
    }else{
        pthread->status = TASK_READY;
    }

    pthread->ticks = 0;
    pthread->elapsed_ticks = 0;
    pthread->pgdir = NULL;
    // self_kstack是栈顶地址,PCB占据1页的大小，设置到最高处。
    pthread->self_kstack = (uint32_t*)((uint32_t)pthread + PG_SIZE);    // pthread地址做一下uint32_t类型转换
    pthread->stack_magic = 0x19870916;      // 自定义的魔数
}

/* 创建名字为name,优先级为priority的，执行function(func_args)的线程，成功则返回对应的PCB */
struct task_struct* thread_start(char* name, int priority, thread_func function, void* func_args){
    // 定义pthread, 在内核中分配1页内存
    struct task_struct* pthread = get_kernel_pages(1);      
    
    init_thread(pthread, name, priority);           // 初始化其他部分为0
    thread_create(pthread, function, func_args);        // 将self_kstack放到栈的栈顶，地址最低端

    ASSERT(!list_search(&thread_ready_list, &pthread->general_tag));
    list_append(&thread_ready_list, &pthread->general_tag);

    ASSERT(!list_search(&thread_all_list, &pthread->all_list_tag));
    list_append(&thread_all_list, &pthread->all_list_tag);
   
// 将栈顶指针self_kstack 放入esp , 线程栈当中用到的管理寄存器，弹出栈保存起来， ret将栈顶的数据返回到EIP当中,开始执行kernel_thread函数的代码
   // asm volatile("movl %0, %%esp; pop %%ebp; pop %%ebx; pop %%edi; pop %%esi; ret": :"g" (pthread->self_kstack) : "memory");    
    return pthread;
}

/* 将kernel当中的main函数完善为主线程 */
static void make_main_thread(void){
    main_thread = running_thread();

    init_thread(main_thread, "main_thread", 31);

    ASSERT(!list_search(&thread_all_list, &main_thread->all_list_tag));
    list_append(&thread_all_list, &main_thread->all_list_tag);
}

/* 初始化线程环境 */
void thread_init(){
    put_str("start init thread\n");
    list_init(&thread_all_list);
    list_init(&thread_ready_list);
    make_main_thread();         // 生成主线程
    put_str("thread_init done\n");
}


void print_thread_list(){
    print_list(&thread_ready_list, thread_tag);
}