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

   printf("/dir2 content before delete /dir2/subdir1:\n");
   sys_mkdir("/dir2");
   sys_mkdir("/dir2/subdir1");
   int32_t inode_no = sys_open("/dir2/subdir1/file1", O_CREAT | O_RDWR);
   sys_close(inode_no);
   struct dir* dir = sys_opendir("/dir2/");
   char* type = NULL;
   struct dir_entry* dir_e = NULL;
   while((dir_e = sys_readdir(dir))) { 
      if (dir_e->f_type == FT_REGULAR) {
	      type = "regular";
      } else {
	      type = "directory";
      }
      printf("      %s   %s\n", type, dir_e->filename);
   }
   printf("try to delete nonempty directory /dir2/subdir1\n");
   if (sys_rmdir("/dir2/subdir1") == -1) {
      printf("sys_rmdir: /dir2/subdir1 delete fail!\n");
   }

   printf("try to delete /dir2/subdir1/file1\n");
   if (sys_rmdir("/dir2/subdir1/file1") == -1) {
      printf("sys_rmdir: /dir1/subdir1/file1 delete fail!\n");
   } 
   if (sys_unlink("/dir2/subdir1/file1") == 0 ) {
      printf("sys_unlink: /dir1/subdir1/file1 delete done\n");
   }
   
   printf("try to delete directory /dir2/subdir1 again\n");
   if (sys_rmdir("/dir2/subdir1") == 0) {
      printf("/dir2/subdir1 delete done!\n");
   }

   printf("/dir2 content after delete /dir2/subdir1:\n");
   sys_rewinddir(dir);
   while((dir_e = sys_readdir(dir))) { 
      if (dir_e->f_type == FT_REGULAR) {
	      type = "regular";
      } else {
	       type = "directory";
      }
      printf("      %s   %s\n", type, dir_e->filename);
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

