#ifndef __KERNEL_MEMORY_H
#define __KERNEL_MEMORY_H
#include "stdint.h"
#include "bitmap.h"
#include "list.h"

#define PG_P_1   1       // 页表项或页目录项存在属性位
#define PG_P_0   0       // 页表项或页目录项存在属性位
#define PG_RW_R  0       // R/W 属性值，读/执行
#define PG_RW_W  2       // R/W 属性位值，读/写/执行
#define PG_US_S  0       // U/S 属性位值，系统级
#define PG_US_U  4       // U/S 属性位值，用户级

#define DESC_INT 7      // 内存块描述符种类

/* 内存块指针 */
struct mem_block{
    struct list_elem free_elem;  
};

/* 内存块描述符,和arena是一对多的关系 */
struct mem_block_desc{ 
    uint32_t mem_block_size;        // 内存块大小
    uint32_t blocks_per_arena;       // 一个arena对应的blocks数量
    struct list free_list;         // 内存块的链表,指向可用的内存块free_elem
};

// 虚拟地址池
struct virtual_addr{
    struct bitmap vaddr_bitmap;     //位图
    uint32_t vaddr_start;       // 起始地址
};

/* 内存池标记，用于判断用哪个内存池 */
enum pool_flags{
    PF_KERNEL = 1,          // 内核内存池
    PF_USER = 2             // 用户内存池
};

extern struct pool kernel_pool, user_pool;
void mem_init(void);
uint32_t* pte_ptr(uint32_t vaddr);
uint32_t* pde_ptr(uint32_t vaddr);
void* malloc_page(enum pool_flags pf, uint32_t pg_cnt);
void* get_kernel_pages(uint32_t pg_cnt);
void* get_user_pages(uint32_t pg_cnt);
void* get_a_page(enum pool_flags pf, uint32_t vaddr);
uint32_t addr_v2p(uint32_t vaddr);
void block_desc_init(struct mem_block_desc *desc_array);
/* 在堆中申请 block_size 字节的内存 */
void* sys_malloc(uint32_t block_size);  
/* 在堆中释放ptr指向的内存 */
void sys_free(void* ptr);
void pfree(uint32_t pg_phy_addr);
void mfree_page(enum pool_flags PF, void* _vaddr, uint32_t pg_cnt);
#endif
