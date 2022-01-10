#include "memory.h"
#include "stdint.h"
#include "print.h"
#include "global.h"
#include "debug.h"
#include "string.h"
#include "thread.h"
#include "sync.h"
#include "../userprog/process.h"

#define PDE_IDX(addr)  (( addr & 0xffc00000) >> 22)
#define PTE_IDX(addr)  (( addr & 0x003ff000) >> 12)

#define PG_SIZE 4096        // 页大小4KB

/* 位图的地址 */
#define MEM_BITMAP_BASE 0xc009a000       // 支持4个位图,512MB内存

/* 动态分配内存的堆地址 */
#define K_HEAP_START 0xc0100000     

struct pool{
    struct bitmap pool_bitmap;      // 物理内存的位图
    uint32_t phy_addr_start;        // 内存池的起始地址
    uint32_t pool_size;         // 内存池的容量,以字节为单位
    // 加锁,保证互斥
    struct lock lock;      // 加锁,保证互斥
};

// 内核物理内存池和用户物理内存池
struct pool kernel_pool, user_pool;
struct virtual_addr kernel_vaddr;       // 用来给内核分配虚拟地址

/* 在pf表示的虚拟内存池当中申请pg_cnt个虚拟页 ,成功则返回虚拟起始地址，失败则返回NULL */
static void* vaddr_get(enum pool_flags pf, uint32_t pg_cnt){
    int vaddr_start = 0, bit_idx_start = -1;
    uint32_t cnt = 0;
    if(pf == PF_KERNEL){        // 内核内存池
        bit_idx_start = bitmap_scan(&kernel_vaddr.vaddr_bitmap, pg_cnt);
        if(bit_idx_start == -1){
            return NULL;
        }
        vaddr_start = kernel_vaddr.vaddr_start + bit_idx_start * PG_SIZE ;
        while(cnt < pg_cnt){
            bitmap_set(&kernel_vaddr.vaddr_bitmap, bit_idx_start + cnt++, 1);
        }
    }
    else{       // 用户内存池, 进程保存有各自的用户虚拟地址池
        struct task_struct* cur = running_thread();         
        bit_idx_start = bitmap_scan(&cur->userprog_vaddr.vaddr_bitmap, pg_cnt); // 获取可用的虚拟内存起始地址
        if(bit_idx_start == -1){
            return NULL;
        }

        while(cnt < pg_cnt){        
            bitmap_set(&cur->userprog_vaddr.vaddr_bitmap, bit_idx_start + (cnt++), 1);
        }
        vaddr_start = cur->userprog_vaddr.vaddr_start + bit_idx_start * PG_SIZE ;
        
        // 注意这里, 用户进程的3特权级栈 (0xc0000000 - PG_SIZE)已经被start_process分配了
        ASSERT((uint32_t)vaddr_start < (0xc0000000 - PG_SIZE));
    }
    return (void*)vaddr_start;
}

/* 得到虚拟地址vaddr对应的pte指针 */
uint32_t* pte_ptr(uint32_t vaddr){
    // 获取访问第1023---0x3ff个页目录表项，指向页目录表本身的页表地址，移动到高10位 --- 0xffc
    // 将PDE的索引找到页表
    // 用PTE的索引作为页内偏移
    uint32_t* pte = (uint32_t*)((0xffc00000) + ((vaddr & 0xffc00000) >> 10) + PTE_IDX(vaddr) * 4 );
    return pte;
}

/* 得到虚拟地址vaddr对应的pde指针 */
uint32_t* pde_ptr(uint32_t vaddr){
    uint32_t* pde = (uint32_t*)( (0xfffff000) + PDE_IDX(vaddr) * 4);
    return pde;
}

/* 在m_pool指向的物理内存池当中分配1个物理页，返回物理页的地址，错误则返回NULL */
static void* palloc(struct pool* m_pool){
    int bit_idx = bitmap_scan(&m_pool->pool_bitmap, 1);
    if(bit_idx == -1){
        return NULL;
    }
    bitmap_set(&m_pool->pool_bitmap, bit_idx, 1);       // 置位为1,表示已使用
    uint32_t page_phyaddr = (uint32_t)(bit_idx * PG_SIZE + m_pool->phy_addr_start);      // 获取物理页的内存地址
    return (void*)page_phyaddr;
}

/* 页表添加虚拟地址_vaddr 和物理地址_page_phyaddr的映射 */
static void page_table_add(void* _vaddr, void* _page_phyaddr){
    uint32_t vaddr = (uint32_t) _vaddr;
    uint32_t phyaddr = (uint32_t) _page_phyaddr;
    // 在页表当中完成映射，本质上是添加页表项PTE

    // 第一步，找到页表,先确认PDE创建完毕,没有则需要先创建PDE（否则会出现page fault）
    // 第二步，构建和安装PTE
    uint32_t* pde = pde_ptr(vaddr);      
    uint32_t* pte = pte_ptr(vaddr);

    if( (*pde) & 0x00000001){        // P=1,存在物理内存当中
        ASSERT( !(*pte & 0x00000001));      // 判断页表是否存在

        if( !(*pte & 0x00000001)){      // 没有，则创建页表
            *pte = (phyaddr | PG_US_U | PG_RW_W | PG_P_1);   // 页表项
        }else{      // 如果存在
            PANIC("pte repeat");        // 已经存在，警告
            //  *pte = (phyaddr | PG_US_U | PG_RW_W | PG_P_1); 
        }
    }else{      // 页目录表项不存在
        // 分配PDE指向的页表的空间
        uint32_t pde_phyaddr = (uint32_t)palloc(&kernel_pool);
        // 先创建PDE
        *pde = (pde_phyaddr | PG_US_U | PG_RW_W | PG_P_1);
        // 将页表 pde_phyaddr 对应的物理页的内容清零, 避免被旧数据扰乱
        // 将pte的高20位映射到pde所指向页表的物理起始地址
        memset((void*)((int)pte & 0xfffff000), 0, PG_SIZE);
        // 设置PTE
        ASSERT(!(*pte & 0x00000001));       // 不应该存在,应该 P=0才对
        *pte = (phyaddr | PG_US_U | PG_RW_W | PG_P_1); 
    }
}

/* 在pf指向的内存池当中分配pg_cnt个页空间，成功或返回起始虚拟地址，失败时返回NULL */
void* malloc_page(enum pool_flags pf, uint32_t pg_cnt){
    // 判断边界情况
    // 虚拟内存分配
    // 物理页的分配，且进行vaddr和phyaddr的映射
    ASSERT( pg_cnt > 0 && pg_cnt < 3840 );     // 判断pg_cnt是否超出地址的限制

    void * vaddr_start = vaddr_get(pf, pg_cnt);   // 获取到的虚拟起始地址
    if(vaddr_start == NULL)
        return NULL;

    struct pool* m_pool = pf & PF_KERNEL ? &kernel_pool : &user_pool;   // & 判断是否相等
    uint32_t vaddr = (uint32_t) vaddr_start;
    int cnt = pg_cnt;

    while( cnt-- > 0 ){               
        void* phy_addr = palloc(m_pool);    // 分配物理内存
        if(phy_addr == NULL){
            return NULL;    
        }
        // 进行地址的映射
        page_table_add((void*) vaddr, phy_addr);     
        vaddr += PG_SIZE;         // 注意每次增长的大小是4KB
    }
    return vaddr_start;
}

/* 从内核物理内存池中申请1页内存，成功则返回虚拟地址，失败则返回NULL */
void* get_kernel_pages(uint32_t pg_cnt){
    lock_acquire(&kernel_pool.lock);
    void* vaddr_start = malloc_page(PF_KERNEL, pg_cnt);
    if(vaddr_start != NULL){    // 不为空，将页框清0后返回
        memset(vaddr_start, 0, pg_cnt * PG_SIZE);
    }
    lock_release(&kernel_pool.lock);
    return vaddr_start;
}

/* 从用户空间当中申请4kB内存,并且返回其虚拟地址 */
void* get_user_pages(uint32_t pg_cnt){
    lock_acquire(&user_pool.lock);
    void* vaddr_start = malloc_page(PF_USER, pg_cnt);
    if(vaddr_start != NULL){
        memset(vaddr_start, 0, pg_cnt * PG_SIZE);
    }
    lock_release(&user_pool.lock);
    return vaddr_start;
}

/* 将vaddr和pf中的物理地址相关联 */
void* get_a_page(enum pool_flags pf, uint32_t vaddr){
    struct pool* mem_pool = pf & PF_KERNEL ? &kernel_pool : &user_pool;
    lock_acquire(&mem_pool->lock);      // 获取锁

    struct task_struct* cur_task = running_thread();
    uint32_t bit_inx = -1;

    /* 用户进程申请用户内存, 设置虚拟内存 */
    if(cur_task->pgdir!=NULL && pf==PF_USER){
        // 修改进程自己的虚拟内存
        bit_inx = (vaddr - cur_task->userprog_vaddr.vaddr_start) / PG_SIZE;
        ASSERT(bit_inx > 0);
        bitmap_set(&cur_task->userprog_vaddr.vaddr_bitmap, bit_inx, 1);

    }else if(cur_task->pgdir == NULL && pf==PF_KERNEL){     /* 内核线程申请内核内存*/
        bit_inx = (vaddr - kernel_vaddr.vaddr_start) / PG_SIZE;     
        ASSERT(bit_inx > 0);
        bitmap_set(&kernel_vaddr.vaddr_bitmap, bit_inx, 1);     // 修改位图对应状态
    }else{
        /* 异常*/
        PANIC("Error in get_a_page, because fail to alloc memory in pool.");
    }
    /* 获取物理内存 */
    void* phy_addr =palloc(mem_pool);
    if(phy_addr==NULL){
        PANIC("FAIL to alloc physic memory.");
        return NULL;
    }
    /* 添加页表的映射 */
    page_table_add((void*)vaddr, phy_addr);    

    lock_release(&mem_pool->lock);      // 释放锁
    return (void*)vaddr;
}

/* 虚拟地址转换为物理地址 */
uint32_t addr_v2p(uint32_t vaddr){
    uint32_t paddr = vaddr & 0x00000fff;
    uint32_t* pte = pte_ptr(vaddr);
    return (( *pte & 0xfffff000) + (paddr));
}

/* 初始化内存池 */
static void mem_pool_init(uint32_t all_mem){
    put_str("mem_pool_init start\n");
    // 页目录表和页表占用的内存大小
    // 页目录表本身有1页的大小 + 目录表中0和768重复指向一个页表 + 769～1022PDE可以指向共254个页表 = 256
    uint32_t page_table_size = PG_SIZE * 256;

    uint32_t used_mem = page_table_size + 0x100000; // 已用内存，加上低端的1MB内存
    uint32_t free_mem = all_mem - used_mem;            // 空闲内存
    uint16_t all_free_pages = free_mem / PG_SIZE;       // 转换成的物理页数

    uint16_t kernel_free_pages = all_free_pages / 2;    //内核与用户对半分
    uint16_t user_free_pages = all_free_pages - kernel_free_pages;

    /* 简化位图操作，余数不作处理，不用做内存的越界检查，因为位图表示出来的内存会小于实际的物理内存
    但是这样会丢失一些内存 */
    uint32_t kbm_length = kernel_free_pages / 8;    // kernel Bitmap的长度，计算位图当中的字节索引
    uint32_t ubm_length = user_free_pages / 8;      // user Bitmap的长度，一位表示一页，通过字节来作为单位
    
    uint32_t kp_start = used_mem;       // Kernel Pool的起始地址
    uint32_t up_start = kp_start + kernel_free_pages * PG_SIZE;  // User Pool的起始地址

    kernel_pool.phy_addr_start = kp_start;
    user_pool.phy_addr_start = up_start;

    kernel_pool.pool_size = kernel_free_pages * PG_SIZE;      
    user_pool.pool_size = user_free_pages * PG_SIZE;

    kernel_pool.pool_bitmap.btmp_bytes_len = kbm_length ;
    user_pool.pool_bitmap.btmp_bytes_len = ubm_length;

/* 内核内存池和用户内存池的位图 */
    kernel_pool.pool_bitmap.bits = (void *)MEM_BITMAP_BASE;     // 内核内存池定义在 0xc009a000
    user_pool.pool_bitmap.bits =(void *)(MEM_BITMAP_BASE + kbm_length);     // 用户内存池跟在内核内存池之后

/*   输出内存池信息 */
    put_str("\t kernel_pool_bitmap_start:");
    put_int((int)kernel_pool.pool_bitmap.bits);
    put_str(" kernel_pool_phy_addr_start:");
    put_int(kernel_pool.phy_addr_start);
    put_str("\n");

    put_str("\t user_pool_bitmap_start:");
    put_int((int)user_pool.pool_bitmap.bits);
    put_str(" user_pool_phy_addr_start:");
    put_int(user_pool.phy_addr_start);
    put_str("\n");

    /* 将位图置位0 */
    bitmap_init(&kernel_pool.pool_bitmap);
    bitmap_init(&user_pool.pool_bitmap);

    /* 初始化内存池的锁 */
    lock_init(&kernel_pool.lock);
    lock_init(&user_pool.lock);

   /* 初始化内核虚拟地址的位图 */ 
    kernel_vaddr.vaddr_bitmap.bits = (void*)(MEM_BITMAP_BASE + kbm_length + ubm_length);
    kernel_vaddr.vaddr_bitmap.btmp_bytes_len = kbm_length;

    kernel_vaddr.vaddr_start = K_HEAP_START;
    bitmap_init(&kernel_vaddr.vaddr_bitmap);
    put_str("  mem_pool_init done \n");
}


/* 内存管理初始化 */
void mem_init(void){
    put_str("mem_init start\n");
    uint32_t mem_bytes_total = (*(uint32_t*)(0xb00));
    mem_pool_init(mem_bytes_total);      // 初始化内存池
    put_str("mem_init done\n");
}

