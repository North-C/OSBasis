#include "print.h"
#include "init.h"
#include "debug.h"
#include "memory.h"
#include "interrupt.h"
#include "../thread/thread.h"
#include "ioqueue.h"
#include "keyboard.h"
#include "console.h"
#include "../userprog/process.h"
#include "list.h"
#include "../userprog/syscall_init.h"
#include "../lib/user/syscall.h"
#include "../lib/stdio.h"

// typedef void(*ptr_to_func)(int);  ptr_to_func signal(int, ptr_to_func);
void k_thread_a(void* arg);
void k_thread_b(void* arg);
void u_thread_a(void);
void u_thread_b(void);
int test_var_a = 0 , test_var_b = 0;
// int prog_a_pid = 0, prog_b_pid = 0;

int main(void){
    put_str("I am kernel\n");
    init_all();
    // 具体的取值和调度、阻塞的时机相关
    enable_intr();
    
    // process_execute(u_thread_a, "user_prog_a");
    // process_execute(u_thread_b, "user_prog_b");
    thread_start("kthread_a", 31, k_thread_a, "I am thread_a ");
    thread_start("kthread_b", 31, k_thread_b, "I am thread_b ");
    while(1);
    return 0;
}


void  k_thread_a(void* arg){
    char *para = arg;
    void *addr = sys_malloc(33);
    console_put_str("I am thread a, sys_malloc(33), addr is 0x");
    console_put_int((int)addr);
    console_put_char('\n');
    while(1);
}

void  k_thread_b(void* arg){
    char *para = arg;
    void *addr = sys_malloc(63);
    console_put_str("I am thread b, sys_malloc(63), addr is 0x");
    console_put_int((int)addr);
    console_put_char('\n');
    while(1);
}

void u_thread_a(void){
    while(1){
        test_var_a++;
    }
}

void u_thread_b(void){
    while(1){
        test_var_b++;
    }
}

