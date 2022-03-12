#ifndef __FS_SUPER_BLOCK_H
#define __FS_SUPER_BLOCK_H
#include "stdint.h"

/* 超级块 */
struct super_block{
    uint32_t magic;         // 魔数，标识文件系统类型
    uint32_t sec_cnt;       // 分区总共的扇区数
    uint32_t inode_cnt;     // 分区中的inode数量
    uint32_t part_lba_base;       // 分区的起始lba地址

    uint32_t block_bitmap_lba;      // 块位图本身的起始扇区地址
    uint32_t block_bitmap_sects;    // 块位图本身占用的扇区数量

    uint32_t inode_bitmap_lba;      // i结点位图起始扇区lba地址
    uint32_t inode_bitmap_sects;        // i结点位图占用的扇区数

    uint32_t inode_table_lba;       // inode表的lba地址
    uint32_t inode_table_sects;         // inode表占用的扇区数量

    uint32_t data_start_lba;        // 数据区开始的第一个扇区号
    uint32_t root_inode_no;         // 根目录所在的inode 号
    uint32_t dir_entry_size;        // 目录项大小，长度

    uint8_t pad[460];       // 凑够512字节，刚好一个扇区大小

}__attribute__ ((packed));      
#endif