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


// typedef void(*ptr_to_func)(int);  ptr_to_func signal(int, ptr_to_func);
void k_thread_a(void* arg);
void k_thread_b(void* arg);
void u_thread_a(void);
void u_thread_b(void);
int test_var_a = 0 , test_var_b = 0;

int main(void){
    put_str("I am kernel\n");
    init_all();
    // ASSERT(1==2);
    //    asm volatile("sti");        // 临时开启中断

    // void *addr = get_kernel_pages(3);
    // put_str("\n get_kernel_page start vaddr is ");
    // put_int((uint32_t) addr);
    // put_str("\n");

    thread_start("k_thread_a", 31, k_thread_a, "argA_");
    thread_start("k_thread_b", 31, k_thread_b, "argB_");

    process_execute(u_thread_a, "user_prog_a");
    process_execute(u_thread_b, "user_prog_b");

    console_put_str(" after process_execute");
    
    enable_intr();      // 打开中断
    console_put_str("after enable_intr");
    while(1); //{
  //      console_put_str("Main ");
        // print_thread_list();
  //  };
    return 0;
}


void  k_thread_a(void* arg){

    // while(1){
    //     enum intr_status old = disable_intr();
    //     if(!ioq_empty(&kbd_buf)){
    //         console_put_str(arg);
    //         char cur_char = ioq_getchar(&kbd_buf);
    //         console_put_char(cur_char);
    //         console_put_char(' ');
    //     }
    //     set_intr_status(old);
    // }
    char* para = arg;
    while(1){
        console_put_str("v_a:0x");
        console_put_int(test_var_a);
        // put_str(para);
    }
}

void  k_thread_b(void* arg){

    //  while(1){
    //     enum intr_status old = disable_intr();
    //     if(!ioq_empty(&kbd_buf)){
    //         console_put_str(arg);
    //         char cur_char = ioq_getchar(&kbd_buf);
    //         console_put_char(cur_char);
    //         console_put_char(' ');
    //     }
    //     set_intr_status(old);
    // }
    char* para = arg;
    while(1){
        console_put_str(" v_b:0x");
        console_put_int(test_var_b);
        //put_str(para);
    }
}

void u_thread_a(void){
    while(1){
        test_var_a++;
        //console_put_str("var_a++\n");
    }
}

void u_thread_b(void){
    while(1){
        test_var_b++;
        //console_put_str("var_b++\n");
    }
}

