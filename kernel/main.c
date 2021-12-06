#include "print.h"
#include "init.h"
#include "debug.h"
#include "memory.h"

// typedef void(*ptr_to_func)(int);  ptr_to_func signal(int, ptr_to_func);

int main(void){
    put_str("I am kernel\n");
    init_all();
    // ASSERT(1==2);
    //    asm volatile("sti");        // 临时开启中断

    void *addr = get_kernel_pages(3);
    put_str("\n get_kernel_page start vaddr is ");
    put_int((uint32_t) addr);
    put_str("\n");

    while(1);
    return 0;
}











