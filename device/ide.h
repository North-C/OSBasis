#ifndef __DEVICE_IDE_H
#define __DEVICE_IDE_H

#include "stdint.h"
#include "sync.h"
#include "../fs/super_block.h"

/* 分区 */
struct partition{
    uint32_t start_lba;         // 起始扇区
    uint32_t sec_cnt;           // 扇区数
    struct disk* my_disk;       // 分区所属的硬盘
    struct list_elem part_tag;      // 用于队列中的标记
    char name[8];               // 分区名称
    struct super_block* sb;         // 本分区的超级块
    struct bitmap block_bitmap;     // 内存块位图
    struct bitmap inode_bitmap;     // i结点位图
    struct list open_inodes;        // 本分区打开的i结点队列
};

/* 硬盘 */
struct disk{
    char name[8];   // 硬盘名称
    struct ide_channel* my_channel;  // 属于哪一个ide通道
    uint8_t dev_no;                    // 硬盘是主盘0，还是从盘1
    struct partition primary_partition[4];      // 主分区不超过4个
    struct partition logical_partition[8];      // 人为限制逻辑分区为8个
};

/* ata通道结构 */
struct ide_channel{
    char name[8];           // 本ata通道名称
    uint16_t port_base;     // 本通道的起始端口号,注意这里只处理两个通道的主板
    uint8_t irq_no;         // 本通道所用的中断号,根据中断号来判断在哪个通道中操作
    struct lock lock;       // 通道锁，注意一个通道只有一个中断信号
    bool expecting_intr;    // 等待硬盘的中断
    struct semaphore disk_done;     // 用于阻塞、唤醒驱动程序
    struct disk devices[2];         // 一个通道连接两个硬盘，分为主从
};

void ide_init(void);
void ide_read(struct disk* hd, uint32_t lba, void* buf, uint32_t sec_cnt);
void ide_write(struct disk* hd, uint32_t lba, void* buf, uint32_t sec_cnt);
void intr_hd_handler(uint8_t irq_no);
extern uint8_t channel_cnt;
extern struct ide_channel channels[];
extern struct list partition_list;
#endif
