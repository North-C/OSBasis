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
    void* addr1;
    void* addr2;
    void* addr3;
    void* addr4;
    void* addr5;
    void* addr6;
    void* addr7;
    console_put_str(" thread_a start\n");
    int max = 1000;
    while(max-- > 0){
        console_put_str(" times: ");
        console_put_int(1000 - max);
        console_put_char('\n');

        int size = 128;
        addr1 = sys_malloc(size);
        size *= 2;
        addr2 = sys_malloc(size);
        size *= 2;
        addr3 = sys_malloc(size);
        sys_free(addr1);
        addr4 = sys_malloc(size);
        size *= 2; size *= 2; size *= 2; size *= 2;
        size *= 2; size *= 2; size *= 2;
        addr5 = sys_malloc(size);
        //console_put_str("\n malloc(addr5) success!");
        addr6 = sys_malloc(size);
        sys_free(addr5);
        size *= 2;
        addr7 = sys_malloc(size);
        sys_free(addr6);
        sys_free(addr7);
        sys_free(addr2);
        sys_free(addr3);
        sys_free(addr4);

        //console_put_str("end\n");
    }
    console_put_str(" thread_a end\n");
    while(1);
}

void  k_thread_b(void* arg){
    char *para = arg;
    void* addr1;
    void* addr2;
    void* addr3;
    void* addr4;
    void* addr5;
    void* addr6;
    void* addr7;
    void* addr8;
    void* addr9;
    int max = 1000;
    //console_put_str(" thread_b start\n");
    while(max-- > 0){
        int size = 9;
        addr1 = sys_malloc(size);
        size *= 2;
        addr2 = sys_malloc(size);
        size *= 2;
        sys_free(addr2);
        addr3 = sys_malloc(size);
        sys_free(addr1);
        addr4 = sys_malloc(size);
        addr5 = sys_malloc(size);
        addr6 = sys_malloc(size);
        sys_free(addr5);
        size *= 2;
        addr7 = sys_malloc(size);
        sys_free(addr6);
        sys_free(addr7);
        sys_free(addr3);
        sys_free(addr4);

        size *= 2; size *= 2; size *= 2;
        addr1 = sys_malloc(size);
        addr2 = sys_malloc(size);
        addr3 = sys_malloc(size);
        addr4 = sys_malloc(size);
        addr5 = sys_malloc(size);
        addr6 = sys_malloc(size);
        addr7 = sys_malloc(size);
        addr8 = sys_malloc(size);
        addr9 = sys_malloc(size);
        sys_free(addr1);
        sys_free(addr2);
        sys_free(addr3);
        sys_free(addr4);
        sys_free(addr5);
        sys_free(addr6);
        sys_free(addr7);
        sys_free(addr8);
        sys_free(addr9);
    }
    console_put_str(" thread_b end\n");
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

