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
#include "../fs/dir.h"

void k_thread_a(void* arg);
void k_thread_b(void* arg);
void u_thread_a(void);
void u_thread_b(void);

int main(void){
    put_str("I am kernel\n");
    init_all();
    struct dir* p_dir = sys_opendir("/dir1/subdir1");
    if(p_dir){
       printf("/dir1/subdir1 open done!\n content:\n ");
       char* type = NULL;
       struct dir_entry* dir_e = NULL;
       while((dir_e = sys_readdir(p_dir))){
          if(dir_e->f_type == FT_REGULAR){
             type = "Regular";
          }else{
             type = "Directory";
          }
          printf("  %s   %s\n", type, dir_e->filename);
       }
       if(sys_closedir(p_dir) == 0){
          printf("/dir1/subdir1 close done!\n");
       }else{
          printf("/dir1/subdir1 close failed!\n");
       }
    }else{
      printf("/dir1/subdir1 open fail!\n");
    }
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

