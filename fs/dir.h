#ifndef __FS_DIR_H
#define __FS_DIR_H

#include "stdint.h"
#include "fs.h"
#include "inode.h"
#include "ide.h"
#include "global.h"
#define MAX_FILE_NAME_LEN 16

extern struct dir root_dir;
/* 目录结构 */
struct dir
{
    struct inode* inode;        // 对应的inode
    uint32_t dir_pos;           // 记录在目录内的偏移
    uint8_t dir_buf[512];       // 目录的数据缓存
};

/* 目录项结构,不跨扇区写入 */
struct dir_entry{
    char filename[MAX_FILE_NAME_LEN];       // 普通文件或目录名称
    uint32_t i_no;                  // 普通文件或目录对应的inode编号
    enum file_types f_type;             // 文件类型
};

/* 打开根目录 */
void open_root_dir(struct partition* part);
/* 打开目录 */
struct dir* dir_open(struct partition* part, uint32_t inode_no);

/* 在part分区内的pdir目录内寻找名为name的文件或目录，找到则返回true，并存入dir_e，失败则返回false */
bool search_dir_entry(struct partition* part, struct dir* pdir, const char* name, \
            struct dir_entry* dir_e);
/* 关闭目录 */
void dir_close(struct dir* dir);
/* 在内存中初始化目录项 p_de */
void create_dir_entry(char* filename, uint32_t inode_no, uint8_t file_type, struct dir_entry* p_de);
/* 将目录项p_de写入父目录 parent_dir中，io_buf由主调函数提供 */
bool sync_dir_entry(struct dir* parent_dir, struct dir_entry* p_de, void* io_buf);

bool delete_dir_entry(struct partition* part, struct dir* pdir, uint32_t inode_no, void* io_buf);
struct dir_entry* dir_read(struct dir* dir);
#endif