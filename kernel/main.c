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
#include "../fs/fs.h"
#include "../lib/string.h"

void k_thread_a(void* arg);
void k_thread_b(void* arg);
void u_thread_a(void);
void u_thread_b(void);

int main(void){
    put_str("I am kernel\n");
    init_all();
    // 具体的取值和调度、阻塞的时机相关    
    process_execute(u_thread_a, "user_prog_a");
    process_execute(u_thread_b, "user_prog_b");
    thread_start("kthread_a", 31, k_thread_a, "I am thread_a ");
    thread_start("kthread_b", 31, k_thread_b, "I am thread_b ");


    uint32_t fd = sys_open("/file1", O_RDWR);
    printf("open /file1,fd:%d\n", fd);
    char buf[64] = {0};
    int read_bytes = sys_read(fd, buf, 18);
    printf("1_ read %d bytes:\n%s\n", read_bytes, buf);

    memset(buf, 0 ,64);
    read_bytes = sys_read(fd, buf,6);
    printf("2_ read %d bytes:\n%s\n", read_bytes, buf);

    memset(buf, 0 ,64);
    read_bytes = sys_read(fd, buf,18);
    printf("3_ read %d bytes:\n%s\n", read_bytes, buf);

    printf("_____ close file1  and reopen _____\n");
    sys_close(fd);
    fd = sys_open("/file1", O_RDWR);
    memset(buf, 0, 64);
    read_bytes = sys_read(fd, buf, 24);
    printf("4_ read %d bytes:\n%s\n", read_bytes, buf);   
    sys_close(fd);

    while(1);
    return 0;
}

void  k_thread_a(void* arg){
   void* addr1 = sys_malloc(256);
   void* addr2 = sys_malloc(255);
   void* addr3 = sys_malloc(254);
   console_put_str(" thread_a malloc addr:0x");
   console_put_int((int)addr1);
   console_put_char(',');
   console_put_int((int)addr2);
   console_put_char(',');
   console_put_int((int)addr3);
   console_put_char('\n');

   int cpu_delay = 10000;
   while(cpu_delay-- > 0);
   sys_free(addr1);
   sys_free(addr2);
   sys_free(addr3);
   while(1);
}

void  k_thread_b(void* arg){
     void* addr1 = sys_malloc(256);
   void* addr2 = sys_malloc(255);
   void* addr3 = sys_malloc(254);
   console_put_str(" thread_b malloc addr:0x");
   console_put_int((int)addr1);
   console_put_char(',');
   console_put_int((int)addr2);
   console_put_char(',');
   console_put_int((int)addr3);
   console_put_char('\n');

   int cpu_delay = 10000;
   while(cpu_delay-- > 0);
   sys_free(addr1);
   sys_free(addr2);
   sys_free(addr3);
   while(1);
}

void u_thread_a(void){
    void* addr1 = malloc(256);
   void* addr2 = malloc(255);
   void* addr3 = malloc(254);
   printf(" prog_a malloc addr:0x%x,0x%x,0x%x\n", (int)addr1, (int)addr2, (int)addr3);

   int cpu_delay = 10000;
   while(cpu_delay-- > 0);
   free(addr1);
   free(addr2);
   free(addr3);
   while(1);
}

void u_thread_b(void){
     void* addr1 = malloc(256);
   void* addr2 = malloc(255);
   void* addr3 = malloc(254);
   printf(" prog_b malloc addr:0x%x,0x%x,0x%x\n", (int)addr1, (int)addr2, (int)addr3);

   int cpu_delay = 10000;
   while(cpu_delay-- > 0);
   free(addr1);
   free(addr2);
   free(addr3);
   while(1);
}

