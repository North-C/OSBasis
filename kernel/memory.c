#include "memory.h"
#include "stdint.h"
#include "print.h"
#include "global.h"
#include "debug.h"
#include "string.h"
#include "thread.h"
#include "sync.h"
#include "../userprog/process.h"
#include "interrupt.h"
#include "list.h"

#define PDE_IDX(addr)  (( addr & 0xffc00000) >> 22)
#define PTE_IDX(addr)  (( addr & 0x003ff000) >> 12)

#define PG_SIZE 4096        // 页大小4KB

/***************  位图地址 ********************
 * 因为0xc009f000是内核主线程栈顶，0xc009e000是内核主线程的pcb.
 * 一个页框大小的位图可表示128M内存, 位图位置安排在地址0xc009a000,
 * 这样本系统最大支持4个页框的位图,即512M内存 */

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

struct mem_block_desc k_block_desc[DESC_INT];   // 内存块描述符的数组

/* 内存仓库arena */
struct arena{
    struct mem_block_desc *desc;        // 关联的内存描述符
    uint32_t cnt;       // large为true，表示页框数；为false,表示空闲的mem_block内存块数目
    bool large;             // 是否为大内存块
};

static void page_table_pte_remove(uint32_t vaddr);
static void vaddr_remove(enum pool_flags PF, void* _vaddr, uint32_t pg_cnt);

/* 在pf表示的虚拟内存池当中申请pg_cnt个虚拟页 ,成功则返回虚拟起始地址，失败则返回NULL */
static void* vaddr_get(enum pool_flags pf, uint32_t pg_cnt){
    int vaddr_start = 0, bit_idx_start = -1;
    uint32_t cnt = 0;
    if(pf == PF_KERNEL){        // 内核内存池
        bit_idx_start = bitmap_scan(&kernel_vaddr.vaddr_bitmap, pg_cnt);
        if(bit_idx_start == -1){
           // put_str(" vir_bit_idx in kernel is -1");
            return NULL;
        }
        // put_str(" vir_bit_idx in kernel:");
        // put_int(bit_idx_start);
        while(cnt < pg_cnt){
            bitmap_set(&kernel_vaddr.vaddr_bitmap, bit_idx_start + cnt++, 1);
        }
        vaddr_start = kernel_vaddr.vaddr_start + bit_idx_start * PG_SIZE ;
    }
    else{       // 用户内存池, 进程保存有各自的用户虚拟地址池
        struct task_struct* cur = running_thread();         
        bit_idx_start = bitmap_scan(&cur->userprog_vaddr.vaddr_bitmap, pg_cnt); // 获取可用的虚拟内存起始地址
        if(bit_idx_start == -1){
            //put_str(" vir_bit_idx in user is -1");
            return NULL;
        }
        // put_str(" vir_bit_idx in user:");
        // put_int(bit_idx_start);
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
    int bit_idx = bitmap_scan(&m_pool->pool_bitmap, 1);     // 寻找大小为1的物理页面
    // put_str(" phy_bit_idx:");
    // put_int(bit_idx);
    if(bit_idx == -1){
        return NULL;
    }
    bitmap_set(&m_pool->pool_bitmap, bit_idx, 1);       // 置位为1,表示已使用
    uint32_t page_phyaddr = (bit_idx * PG_SIZE + m_pool->phy_addr_start);      // 获取物理页的内存地址
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
        ASSERT( !(*pte & 0x00000001));      // 判断页表是否已经存在

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
    if(vaddr_start == NULL){
        return NULL;
    }

    struct pool* m_pool = pf & PF_KERNEL ? &kernel_pool : &user_pool;   // & 判断是否相等
    uint32_t vaddr = (uint32_t) vaddr_start;
    uint32_t cnt = pg_cnt;

    while( cnt-- > 0 ){               
        void* phy_addr = palloc(m_pool);    // 分配物理内存
        if(phy_addr == NULL){
            return NULL;    
        }
        // // 进行地址的映射
        // put_str(" vaddr: 0x");
        // put_int((uint32_t) vaddr);
        // put_char('\n');

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
    }else{
        PANIC("malloc_user_page error");
    }
    lock_release(&user_pool.lock);
    return vaddr_start;
}

/* 将vaddr和pf中的物理地址相关联 */
void* get_a_page(enum pool_flags pf, uint32_t vaddr){
    struct pool* mem_pool = pf & PF_KERNEL ? &kernel_pool : &user_pool;
    lock_acquire(&mem_pool->lock);      // 获取锁

    struct task_struct* cur_task = running_thread();
    uint32_t bit_idx = -1;

    /* 用户进程申请用户内存, 设置虚拟内存 */
    if(cur_task->pgdir!=NULL && pf==PF_USER){
        // 修改进程自己的虚拟内存
        bit_idx = (vaddr - cur_task->userprog_vaddr.vaddr_start) / PG_SIZE;
        ASSERT(bit_idx > 0);
        bitmap_set(&cur_task->userprog_vaddr.vaddr_bitmap, bit_idx, 1);

    }else if(cur_task->pgdir == NULL && pf==PF_KERNEL){     /* 内核线程申请内核内存*/
        bit_idx = (vaddr - kernel_vaddr.vaddr_start) / PG_SIZE;     
        ASSERT(bit_idx > 0);
        bitmap_set(&kernel_vaddr.vaddr_bitmap, bit_idx, 1);     // 修改位图对应状态
    }else{
        /* 异常*/
        PANIC("Error in get_a_page, because fail to alloc memory in pool.");
    }
    /* 获取物理内存 */
    void* phy_addr =palloc(mem_pool);
    if(phy_addr==NULL){
        PANIC("FAIL to alloc physic memory.");
        lock_release(&mem_pool->lock);
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

    uint32_t used_mem = page_table_size + 0x100000; // 已用内存，加上低端的1MB内存, 为0x200000
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

 

/* 返回arena当中第block_idx个内存块 */
static struct mem_block* arena2block(struct arena* ar, uint32_t block_idx){
    return (struct mem_block*) ((uint32_t)ar + sizeof(struct arena) + ar->desc->mem_block_size * block_idx);
}

/* 返回内存块描述符所对应的arena*/
static struct arena* block2arena(struct mem_block* block){
    return (struct arena*)((uint32_t)block & 0xfffff000);       // 直接取页的基地址即可
}

/* 分配block_size字节的内存，返回对应内存块block的地址 */
void* sys_malloc(uint32_t block_size){

    struct mem_block_desc *block_desc = NULL;
    struct task_struct* cur_thread = running_thread();
    struct pool* mem_pool;
    uint32_t pool_size = 0;     // 内存池的容量
    enum pool_flags PF;         // 内存池的标记

    /*首先给arena和内存块本身分配内存，判断使用哪一个内存池 */
    if(cur_thread->pgdir == NULL){  // 内核进程
        PF = PF_KERNEL;
        block_desc = k_block_desc;
        mem_pool = &kernel_pool;
        pool_size = mem_pool->pool_size;
    }
    else{       // 用户进程
        PF = PF_USER;
        block_desc = cur_thread->u_block_desc;
        mem_pool = &user_pool;
        pool_size = mem_pool->pool_size;
    }
    /* 不在内存池的范围之内 */
    if (block_size <= 0 || block_size >= pool_size){
        return NULL;
    }

    struct arena* ar;
    // struct mem_block_desc* block;
    //uint32_t pg_cnt;        // 需要分配的page的数目
    struct mem_block* block;
    
    lock_acquire(&mem_pool->lock);
    
    /* 判断需要分配的页框大小 */
    if(block_size > 1024){      // 大内存块，直接分配page
        uint32_t pg_cnt = DIV_ROUND_UP(block_size + sizeof(struct arena), PG_SIZE);   // 向上取整，计算分配几个页面
        ar = malloc_page(PF, pg_cnt);
        //put_str("malloc_page() success!");

        if(ar == NULL){
            lock_release(&mem_pool->lock);         // 释放锁
            return NULL;
        }else{
            // 进行初始化,对于分配的大块页框,将desc置为NULL, cnt置为页框数,large置为true
            memset(ar, 0, PG_SIZE * pg_cnt);
            ar->cnt = pg_cnt;       // 以页框为单位
            ar->large = true;
            ar->desc = NULL;        // 区别：大页的desc不设置
            lock_release(&mem_pool->lock);
            return (void*)(ar+1);           // 跨过arena的元信息，返回空闲内存块地址
        }
    }
    else{                   // 按照不同的内存块描述符
        uint32_t desc_idx ;
        for(desc_idx = 0; desc_idx < DESC_INT; desc_idx++){
            if( block_size <= block_desc[desc_idx].mem_block_size){
                break;
            }
        }
        // if( desc_idx >= DESC_INT){
        //     lock_release(&mem_pool->lock);
        //     return NULL;
        // }

        /* 缺少可用的mem_block： */
        if(list_empty(&block_desc[desc_idx].free_list)){
            // 分配arena的空间
            ar = malloc_page(PF, 1);
            if(ar==NULL){
                //put_str("malloc_page() fail!");
                lock_release(&mem_pool->lock);
                return NULL;
            }
            //put_str("malloc_page() success!");
            memset(ar, 0, PG_SIZE);     // 清零
            // cnt设置为arena可用的内存块
            ar->cnt = block_desc[desc_idx].blocks_per_arena;      // 在初始化的时候已经计算过
            ar->large = false;
            ar->desc = block_desc + desc_idx;
        
            // 拆分arena为内存块,同时添加到内存块desc的free_list当中
            enum intr_status old_status = disable_intr();   // 关闭中断
            uint32_t block_idx;
            
            for(block_idx = 0; block_idx < ar->cnt ; block_idx++){
                block = arena2block(ar, block_idx);
                ASSERT(!list_search(&ar->desc->free_list,  &block->free_elem));     // 判断是否被已经存在，报错
                list_append(&ar->desc->free_list, &block->free_elem);       // 加入可用空闲列表
            }

            set_intr_status(old_status);
        }
        // 分配arena内存，获取更多的mem_block
        
        /* arena进行内存块分配 */
        // 生成内存块
        block = elem2entry( list_pop(&block_desc[desc_idx].free_list), struct mem_block, free_elem);
        memset(block, 0, block_desc[desc_idx].mem_block_size);      // 初始化清零

        ar = block2arena(block);
        ar->cnt--;

        //put_str("allocate block success! ");
        lock_release(&mem_pool->lock);
        return (void*)block;
    }
}

/* 释放ptr指定的任意大小内存 */
void sys_free(void* ptr){
    ASSERT(ptr!=NULL);
    if(ptr!=NULL){
        enum pool_flags PF = PF_KERNEL;
        struct task_struct *cur_thread = running_thread();
        //void *vaddr = ptr;
        uint32_t pg_cnt = sizeof(*ptr);
        struct pool *mem_pool ;
        if(cur_thread->pgdir == NULL){  // 是进程
            PF = PF_KERNEL;
            mem_pool = & kernel_pool;
        }else{      // 是线程
            PF = PF_USER;
            mem_pool = & user_pool;
        }
        
        lock_acquire(&mem_pool->lock);
        struct mem_block *block = ptr;
        struct arena * a = block2arena(ptr);

        ASSERT(a->large == 0 || a->large == 1);
        if((a->large==true) && (a->desc == NULL)){  // 大内存块 > 1024B
            // 释放 页为单位的大内存 内存池清零和映射消除
            mfree_page(PF, a, a->cnt);
        }
        else{
            // 回收小于页的内存块  加入arena的freelist
            list_append(&a->desc->free_list, &block->free_elem);

            // 判断是否arena都空闲，可以将其全部释放
            if( ++a->cnt == a->desc->blocks_per_arena){ 
                uint32_t block_idx;
                for(block_idx = 0; block_idx < a->desc->blocks_per_arena; block_idx++){
                    struct mem_block* b = arena2block(a, block_idx);
                    ASSERT(list_search(&a->desc->free_list, &b->free_elem));            
                    list_remove(&b->free_elem);         // 从空闲列表中删除arena 所包含的内存块
                }
                mfree_page(PF, a, 1);
            }
        }
        lock_release(&mem_pool->lock);
    }
}
/* 释放以虚拟地址vaddr为起始的cnt个物理页框 */
void mfree_page(enum pool_flags PF, void* _vaddr, uint32_t pg_cnt){
    uint32_t page_cnt  = 0;
    uint32_t vaddr = (int32_t)_vaddr;

    ASSERT((pg_cnt > 0) && (vaddr % PG_SIZE == 0));
    /* 
    * 删除物理内存 --> 删除页表映射 --> 删除虚拟内存池
    */
   // 获取_vaddr对应的物理内存
    uint32_t pg_phy_addr = addr_v2p(vaddr);

    // 确保在低端1MB + 1KB大小的页目录表 + 1KB大小页表范围之外--- 0x10200000 , 内存池则从0x2000000开始
    ASSERT( ( pg_phy_addr >= 0x102000 && (pg_phy_addr % PG_SIZE) == 0 )); // 单位是 页 4KB

    if(pg_phy_addr >= user_pool.phy_addr_start){       // 在用户内存池
        
        vaddr -= PG_SIZE;           // ????
        while( page_cnt < pg_cnt){
            vaddr += PG_SIZE;
            pg_phy_addr = addr_v2p(vaddr);
            ASSERT( (pg_phy_addr >= user_pool.phy_addr_start) && (pg_phy_addr % PG_SIZE) == 0 );
            
            pfree(pg_phy_addr);
            page_table_pte_remove(vaddr);

            page_cnt++;
        }
        vaddr_remove( PF, _vaddr, pg_cnt);

    }
    else{       // 内核内存池
        vaddr -= PG_SIZE;           // ????
        while( page_cnt < pg_cnt){
            vaddr += PG_SIZE;
            pg_phy_addr = addr_v2p(vaddr);
            ASSERT( (pg_phy_addr >= kernel_pool.phy_addr_start) && (pg_phy_addr % PG_SIZE) == 0 && (pg_phy_addr < user_pool.phy_addr_start));
            
            pfree(pg_phy_addr);
            page_table_pte_remove(vaddr);
            page_cnt++;
        }
        vaddr_remove( PF, _vaddr, pg_cnt);
    }
}

/* 释放物理内存页pg_ohy_addr，会受到物理内存池当中 */
void pfree(uint32_t pg_phy_addr){
    struct pool *mem_pool;
    uint32_t bit_idx = 0;
    /* 判断是用户还是内核 */
    if(pg_phy_addr >= user_pool.phy_addr_start){    
        mem_pool = &user_pool;
    }else{
        mem_pool = &kernel_pool;
    }
    bit_idx = (pg_phy_addr - mem_pool->phy_addr_start) / PG_SIZE;
    bitmap_set(&mem_pool->pool_bitmap, bit_idx, 0);
}

/* 删除页表中的vaddr的映射,设置pte的P位为0 */
static void page_table_pte_remove(uint32_t vaddr){
    // 获取对应的pte
    uint32_t *pte = pte_ptr(vaddr);
    *pte &= ~PG_P_1;     // 将P位置位0
    asm volatile("invlpg %0"::"m" (vaddr): "memory");  // 刷新指定vaddr对应的TLB
}

/* 释放虚拟内存_vaddr起始的连续pg_cnt个虚拟地址 */
static void vaddr_remove(enum pool_flags PF, void* _vaddr, uint32_t pg_cnt){
    uint32_t bit_idx = 0, cnt = 0;
    /* 判断位于 内核虚拟内存池 还是 用户虚拟内存池 */
    if( PF == PF_KERNEL){
        bit_idx = ((uint32_t)_vaddr - kernel_vaddr.vaddr_start) / PG_SIZE;
        while(cnt < pg_cnt){
            bitmap_set(&kernel_vaddr.vaddr_bitmap, bit_idx + cnt++, 0);
        }
    }else{
        struct task_struct* cur_thread = running_thread();    
        bit_idx = ((uint32_t)_vaddr - cur_thread->userprog_vaddr.vaddr_start) / PG_SIZE;
        while(cnt < pg_cnt){
            bitmap_set(&cur_thread->userprog_vaddr.vaddr_bitmap, bit_idx + cnt++, 0);
        }
    }
}

/* 初始化内存块描述符管理 */
void block_desc_init(struct mem_block_desc *desc_array){
    uint32_t block_size = 16;
    uint32_t desc_idx;
    // 初始化每个mem_block_desc描述符
    for(desc_idx = 0; desc_idx < DESC_INT; desc_idx++){
        desc_array[desc_idx].mem_block_size = block_size;
        desc_array[desc_idx].blocks_per_arena = (uint32_t)((PG_SIZE - sizeof(struct arena))/block_size);
        list_init(&desc_array[desc_idx].free_list);
        block_size *= 2;    
    }
}  

/* 内存管理初始化 */
void mem_init(void){
    put_str("mem_init start\n");
    uint32_t mem_bytes_total = (*(uint32_t*)(0xb00));
    mem_pool_init(mem_bytes_total);      // 初始化内存池
    block_desc_init(k_block_desc);      // 初始化内存块描述符
    put_str("mem_init done\n");
}