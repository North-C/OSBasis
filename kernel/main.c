#include "print.h"
#include "init.h"
#include "debug.h"
#include "memory.h"
#include "../thread/thread.h"
// typedef void(*ptr_to_func)(int);  ptr_to_func signal(int, ptr_to_func);
void k_thread_a(void*);

int main(void){
    put_str("I am kernel\n");
    init_all();
    // ASSERT(1==2);
    //    asm volatile("sti");        // 临时开启中断

    // void *addr = get_kernel_pages(3);
    // put_str("\n get_kernel_page start vaddr is ");
    // put_int((uint32_t) addr);
    // put_str("\n"); 

   thread_start("k_thread_a", 31, k_thread_a, "argA ");
    while(1);
    return 0;
}

void  k_thread_a(void* arg){
    while(1){
        put_str((char*)arg);
    }
}








