#ifndef __FS_INODE_H
#define __FS_INODE_H
#include "stdint.h"
#include "list.h"

/* inode结构 */
struct inode{
    uint32_t  i_no;     // inode编号
    uint32_t i_size;        // inode为文件，即文件大小；目录项，即所有 目录entry 大小之和

    uint32_t i_open_cnts;   // 记录此文件被打开的次数
    bool write_deny;        // 写文件不能并行

    uint32_t i_sectors[13];     //0-11为直接块，12为间接指针
    struct list_elem inode_tag;     // inode结点的表示
};
#endif