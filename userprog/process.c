#include "process.h"
#include "interrupt.h"
#include "list.h"
#include "../thread/thread.h"
#include "../kernel/memory.h"
#include "console.h"
#include "string.h"
#include "tss.h"
#include "memory.h"
#include "debug.h"

extern void intr_exit(void);
extern struct list thread_ready_list;      // 线程就绪队列
extern struct list thread_all_list;        // 所有任务队列

/* 构建用户进程初始上下文信息*/
void start_process(void* filename_){
    void* function = filename_;  
    struct task_struct* cur = running_thread();
    cur->self_kstack += sizeof(struct thread_stack) ;   // 越过thread_stack, thread_stack保存上下文

    struct intr_stack* proc_stack = (struct intr_stack*)cur->self_kstack;   // 中断栈,在intr_exit时踏出用户进程上下文
    proc_stack->edi = proc_stack->esi = proc_stack->ebp = proc_stack->esp_dummy = 0;

    proc_stack->eax = proc_stack->ebx = proc_stack->ecx = proc_stack->edx = 0;
    proc_stack->gs = 0;     // 显存 设置为 0
    proc_stack->ds = proc_stack->es = proc_stack->fs = SELECTOR_U_DATA;
    proc_stack->eip = function;     //等待执行的用户程序地址
    proc_stack->cs = SELECTOR_U_CODE;
    proc_stack->eflags = (EFLAGS_IOPL_0 | EFLAGS_MBS | EFLAGS_IF_1 );
    proc_stack->esp = (void*)((uint32_t)get_a_page(PF_USER, USER_STACK3_VADDR) + PG_SIZE);      // 用户的3特权级栈，获取栈的下边界（栈顶地址） + 栈大小 = 栈底地址
    proc_stack->ss = SELECTOR_U_STACK;
    asm volatile("movl %0, %%esp; jmp intr_exit": :"g"(proc_stack): "memory");
}

/* 激活页表,加载到cr3使其生效 */
void page_dir_activate(struct task_struct* p_thread){
    // 注意调用该函数的对象可能并不相同,是进程或者线程
    uint32_t pagedir_phy_addr = 0x100000;    // 内核线程则重新设置为0x100000,物理页表最开始
    
    if(p_thread->pgdir !=NULL ){    // 线程没有初始化页表
        pagedir_phy_addr = addr_v2p((uint32_t)p_thread->pgdir);     // 这里需要获取的是用户进程的页目录表基地址,从void*转换为uint32_t
    }
    // 加载页目录表基地址,使得新页表生效
    asm volatile("movl %0, %%cr3": :"r"(pagedir_phy_addr): "memory");
}

/* 激活线程或进程的页表,更新tss中的esp0为进程的0特权级栈 */
void process_activate(struct task_struct* p_thread){
    ASSERT(p_thread!=NULL);
    /* 给页表分分配内存, 激活页表 */
    
    page_dir_activate(p_thread);
    /* 更新栈 */
    if(p_thread->pgdir){    // 内核线程不需要更改
        update_tss_esp(p_thread);
    }
}

/* 创建页目录表, 将内核页目录表复制给用户页目录表
    成功则返回页目录的虚拟地址,失败则返回-1 */
uint32_t* create_page_dir(void){    
    // 将内核部分的页目录表项作为访问的入口,实现用户进程的内核空间共享
    /* 不能让用户进程直接访问,需要在kernel当中申请page */
    uint32_t* page_dir_vaddr = get_kernel_pages(1);
    if(page_dir_vaddr == NULL){
        console_put_str("create_page_dir: get_kernel_pages failed");
        return NULL;
    }

    /* 让用户空间的高1G页表都指向 对应的内核空间页表 */
    /** 1. 先复制页表 将内核表项复制到用户页目录表第768项之后 **/       
    memcpy( (uint32_t*)((uint32_t)page_dir_vaddr + 0x300*4), (uint32_t*)(0xfffff000 + 0x300*4), 1024);

    /** 2. 更新页目录地址 **/
    uint32_t new_phy_addr = addr_v2p((uint32_t)page_dir_vaddr);       
    page_dir_vaddr[1023] = new_phy_addr | PG_US_U | PG_RW_W | PG_P_1;    // 页目录地址保存在最后一项
    
    // console_put_str("\ncreate_page_dir ");
    // console_put_int(new_phy_addr);
    // console_put_char('\n');

    return page_dir_vaddr;
}

/* 创建用户进程user_prog当中的 userprog_vaddr 虚拟地址位图 */
void create_user_vaddr_bitmap(struct task_struct* user_prog){
    // 在虚拟内存池中进行堆的统一管理
    user_prog->userprog_vaddr.vaddr_start = USER_VADDR_START; // 起始位置vaddr_start 定义在 0x8048000
    // 计算需要给位图分配多大的内存
    uint32_t bitmap_pg_cnt = DIV_ROUND_UP((0xc0000000 - USER_VADDR_START)/PG_SIZE/8, PG_SIZE);

    user_prog->userprog_vaddr.vaddr_bitmap.btmp_bytes_len = (0xc0000000 - USER_VADDR_START)/PG_SIZE/8;
    // 分配对应大小的内存
    user_prog->userprog_vaddr.vaddr_bitmap.bits = get_kernel_pages(bitmap_pg_cnt);
    // 初始化位图
    bitmap_init(&user_prog->userprog_vaddr.vaddr_bitmap);
}

/* 创建用户进程 */
void process_execute(void* filename, char* name){
    struct task_struct* thread = get_kernel_pages(1);   // 内核PCB,在内核中维护，在内核中申请
    init_thread(thread, name, default_prio);        // 初始化线程
    create_user_vaddr_bitmap(thread);       // 分配用户虚拟地址空间

    // 将start_process作为执行函数
    thread_create(thread, start_process, filename);        // 将self_kstack放到栈的栈顶，地址最低端
    thread->pgdir = create_page_dir();      // 页表配置
    
    block_desc_init(thread->u_block_desc);          // 初始化内存分配描述符

    // console_put_str(" thread name is ");
    // console_put_str(thread->name);
    // console_put_str(" thread pagedir is ");
    // console_put_int(thread->pgdir);

    enum intr_status old = disable_intr();  //关闭中断

    // 加入到任务队列
    ASSERT(!list_search(&thread_ready_list, &thread->general_tag));
    list_append(&thread_ready_list, &thread->general_tag);

    ASSERT(!list_search(&thread_all_list, &thread->all_list_tag));
    list_append(&thread_all_list, &thread->all_list_tag);

    // 恢复中断状态
    set_intr_status(old);
}

