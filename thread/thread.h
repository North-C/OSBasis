#ifndef __THREAD_THREAD_H
#define __THREAD_THREAD_H
#include "../lib/stdint.h"
#include "list.h"
#include "bitmap.h"
#include "memory.h"

#define MAX_FILES_OPEN_PER_PROC 8

/* 自定义通用函数类型 */
typedef void thread_func(void*);    // 定义返回值为void，参数为void*的函数thread_func

/* 进程或线程的状态 */ 
enum task_status{
    TASK_RUNNING,       // 运行
    TASK_READY,         // 就绪
    TASK_BLOCKED,       // 阻塞
    TASK_WAITING,       // 等待
    TASK_HANGING,       // 挂起
    TASK_DIED           // 终止
};

/* 中断栈intr_stack  
用于中断发生时保护程序的上下文环境*/ 
struct intr_stack{      // kernel.S的intr%1entry中断入口程序涉及的上下文保护
    uint32_t vec_no;        
    uint32_t edi;
    uint32_t esi;
    uint32_t ebp;
    uint32_t esp_dummy;
// pushad把esp压入，但是popad会忽略它，因为esp始终在变化
    uint32_t ebx;
    uint32_t edx;
    uint32_t ecx;
    uint32_t eax;
    uint32_t gs;
    uint32_t fs;
    uint32_t es;
    uint32_t ds;

/* cpu从低特权级进入高特权级时压入栈 */
    uint32_t err_code;       // 错误码被压入eip之后
    void (*eip) (void);     // -----------------------
    uint32_t cs;
    uint32_t eflags;
    void* esp;        
    uint32_t ss;
};

/* 线程栈 thread_stack，用于存储线程自己带执行的函数 */
struct thread_stack{        // switch_to的时候使用
    uint32_t ebp;
    uint32_t ebx;
    uint32_t edi;
    uint32_t esi;   
// 线程第一次执行，eip执行待调用的函数kernel_thread,其他时候指向switch_to的地址
    void (*eip) (thread_func* func, void* func_arg);

/* 以下仅仅供第一次被调度上cpu时使用 */
    void (*unused_retaddr);     // 充当返回地址,在返回地址的栈帧占个位置
    thread_func* function;      // kernel_thread所调用的函数名
    void* func_arg;   // kernel_thread调用函数所需要的参数, void* 表示所有参数类型
};

/* 进程或者线程的PCB */
struct task_struct{
    uint32_t* self_kstack;      // 内核栈顶指针,位置在结构体的最低端，但是指向PCB的最高端的栈
    pid_t pid;      // 进程ID
    enum task_status status;       // 状态 
    char name[16];          //进程/线程的名字
    uint8_t priority;           // 优先级
    uint8_t ticks;          // 处理器上执行的时钟数

    uint32_t elapsed_ticks;     // 任务从开始到现在执行了多久，占用了多少ticks
    int32_t fd_table[MAX_FILES_OPEN_PER_PROC];          // 文件描述符数组,-1表示为空
    struct list_elem general_tag;       // 线程在队列当中的标记
    struct list_elem all_list_tag;      // 线程队列thread_all_list中的节点

    uint32_t* pgdir;            // 进程自己页表的虚拟地址，只有进程有页表，而线程沒有页表
    struct virtual_addr userprog_vaddr;     //用户进程的虚拟地址
    struct mem_block_desc u_block_desc[DESC_INT];        // 用户进程的内存块描述符数组
    uint32_t stack_magic;       // 魔数设计为0x19870916，栈的边界标记，用于检测栈的溢出。中断处理时可以检测是否PCB被初始化
};
extern struct list thread_ready_list;
extern struct list thread_all_list;

void thread_create(struct task_struct* pthread, thread_func function, void* func_arg);
void init_thread(struct task_struct* pthread, char* name, int priority);
struct task_struct* thread_start(char* name, int priority, thread_func function, void* func_args);
struct task_struct* running_thread(void);
void schedule(void);
void thread_init(void);

void thread_block(enum task_status stat);
void thread_unblock(struct task_struct* pthread);
void thread_yield(void);
#endif