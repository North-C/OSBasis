#include "inode.h"
#include "fs.h"
#include "ide.h"
#include "memory.h"
#include "interrupt.h"
#include "debug.h"
#include "string.h"

// 存储inode在硬盘中的位置
struct inode_position{
    bool two_sec;       // 是否横跨两个扇区
    uint32_t sec_lba;       // 扇区的起始地址
    uint32_t off_size;      // 在扇区内的字节偏移量
};

/* 获取inode所在的扇区和扇区内的偏移量 */
static void inode_locate(struct partition* part, uint32_t inode_no, struct inode_position* inode_pos){
    ASSERT(inode_no < 4096);            // 分区最大4096个文件，inode需要保持分区内的连续

    uint32_t inode_table_lba = part->sb->inode_table_lba ;
    uint32_t inode_size = sizeof(struct inode);
    uint32_t inode_off_size_in_bytes = inode_no * inode_size;
    
    // 扇区为单位的偏移量
    uint32_t inode_off_sec = inode_off_size_in_bytes / 512;
    // 有待查找inode所在扇区的起始地址
    uint32_t off_in_sec = inode_off_size_in_bytes % 512;        // ？？

    // 判断是否跨越两个扇区
    uint32_t left_size = 512 - off_in_sec ;
    if(left_size < inode_size){
        inode_pos->two_sec = true;
    }else{
        inode_pos->two_sec = false;
    }

    inode_pos->sec_lba = inode_table_lba + inode_off_sec;
    inode_pos->off_size = off_in_sec;
}

/* 将inode同步到硬盘分区part当中 */
void inode_sync(struct partition* part, struct inode* inode, void* io_buf){
    // io_buf时用于硬盘io的缓冲区
    uint8_t inode_no = inode->i_no;
    struct inode_position inode_pos;
    // 定位inode在硬盘中的位置
    inode_locate(part, inode_no, &inode_pos);

    // inode位置信息会存入inode_pos
    ASSERT(inode_pos.sec_lba <= (part->start_lba + part->sec_cnt));
    // 存储inode信息
    struct inode pure_inode;
    memcpy(&pure_inode, inode, sizeof(struct inode));
/* inode以下的三个成员只存在于内存当中 */
    pure_inode.i_open_cnts = 0;
    pure_inode.write_deny = false;      // 保证硬盘中读出来后为 可写状态
    pure_inode.inode_tag.front = pure_inode.inode_tag.next = NULL;

    // io_buf作为缓冲区
    char* inode_buf = (char*) io_buf;
    if(inode_pos.two_sec){      // 是否跨两个扇区
        // 读出两个扇区再写入两个扇区
        ide_read(part->my_disk, inode_pos.sec_lba, inode_buf, 2);
        // 将待写入的inode加入到读取出来的两个扇区
        memcpy((inode_buf + inode_pos.off_size), &pure_inode, sizeof(struct inode));
        // 写入硬盘
        ide_write(part->my_disk, inode_pos.sec_lba, inode_buf, 2);

    }else{              // 一个扇区
        ide_read(part->my_disk, inode_pos.sec_lba, inode_buf, 1);
        memcpy((inode_buf + inode_pos.off_size), &pure_inode, sizeof(struct inode));
        ide_write(part->my_disk, inode_pos.sec_lba, inode_buf, 1);
    }

}

/* 根据inode号返回相应的i结点 */
struct inode* inode_open(struct partition* part, uint32_t inode_no){
    // 从open_inodes链表当中查找，这是为提升速度创建的缓冲区
    struct list_elem* elem = part->open_inodes.head.next;       
    struct inode* inode_found;
    while(elem != &part->open_inodes.tail){
        inode_found = elem2entry(elem, struct inode, inode_tag);
        if(inode_found->i_no == inode_no){
            inode_found->i_open_cnts++;
            return inode_found;
        }
        elem = elem->next;
    }

    //从硬盘中读入inode并加入到此链表
    struct inode_position inode_pos;
    // 将inode位置信息存入inode_pos，包括inode所在扇区地址和扇区内的字节偏移量
    inode_locate(part, inode_no, &inode_pos);

    // sys_malloc用于创建新的inode，为了保持inode的共享，将其创建在内核空间当中
    // 因此临时将cur_pcb->pgdir置为NULL
    struct task_struct* cur = running_thread();
    uint32_t* cur_pagedir_bak = cur->pgdir;
    cur->pgdir = NULL;
    inode_found = (struct inode*)sys_malloc(sizeof(struct inode));
    // 恢复pgdir
    cur->pgdir = cur_pagedir_bak;

    char* inode_buf;
    if(inode_pos.two_sec){      // 考虑跨扇区的情况
        inode_buf = (char*) sys_malloc(1024);       // 两个扇区的大小
        ide_read(part->my_disk, inode_pos.sec_lba, inode_buf, 2);
    }else{
        inode_buf = (char*)sys_malloc(512);
        ide_read(part->my_disk, inode_pos.sec_lba, inode_buf, 1);
    }
    memcpy(inode_found, inode_buf+inode_pos.off_size, sizeof(struct inode));

    // 加入到open_inodes链表的头部，可能很快会用到
    list_push(&part->open_inodes, &inode_found->inode_tag);
    inode_found->i_open_cnts = 1;


    sys_free(inode_buf);
    return inode_found;
}

/* 关闭inode或者减少inode的打开数 */
void inode_close(struct inode* inode){
    // 若没有进程再打开此文件，将此inode去掉并释放
    enum intr_status old_status = disable_intr();
    // 从list当中剔除掉
    if(--inode->i_open_cnts == 0){
        list_remove(&inode->inode_tag);
        struct task_struct* cur = running_thread();
        uint32_t* cur_pagdir_bak =  cur->pgdir;
        cur->pgdir = NULL;
        sys_free(inode);
        cur->pgdir = cur_pagdir_bak;
    }
    set_intr_status(old_status);
}

/* 初始化新的inode */
void inode_init(uint32_t inode_no, struct inode* new_inode){
    new_inode->i_no = inode_no;
    new_inode->i_size = 0;
    new_inode->i_open_cnts = 0;
    new_inode->write_deny = true;
    
    // 初始化索引数组 i_sector
    uint8_t sector_idx = 0;
    while(sector_idx < 13){
        new_inode->i_sectors[sector_idx] = 0;
        sector_idx++;
    }
}

