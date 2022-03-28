#include "dir.h"
#include "inode.h"
#include "memory.h"
#include "ide.h"
#include "file.h"
#include "fs.h"
#include "debug.h"
#include "string.h"
#include "stdio-kernel.h"
struct dir root_dir;        // 根目录,处于

/* 打开根目录 */
void open_root_dir(struct partition* part){
    root_dir.inode = inode_open(part, part->sb->root_inode_no);
    root_dir.dir_pos = 0;
}

/* 在分区part上打开i结点为inode_no的目录并返回目录指针 */
struct dir* dir_open(struct partition* part, uint32_t inode_no){
    struct dir* pdir = (struct dir*) sys_malloc(sizeof(struct dir));
    pdir->inode = inode_open(part, inode_no);
    pdir->dir_pos = 0;
    return pdir;
}

/* 在part分区内的pdir目录内寻找名为name的文件或目录，找到则返回true，并存入dir_e，失败则返回false */
bool search_dir_entry(struct partition* part, struct dir* pdir, const char* name,
                 struct dir_entry* dir_e){
    uint32_t block_cnt = 140;    // 12 直接块 + 128一级间接块指向的直接块 = 140
    // 记录块的扇区起始地址
    uint32_t* all_blocks = (uint32_t*)sys_malloc(48+512);
    if(all_blocks == NULL){
        printk("search_dir_entry: sys_malloc for all blocks failed");
        return false;
    }
    // 接收直接块的地址
    uint32_t block_idx = 0;
    while(block_idx < 12){
        all_blocks[block_idx] = pdir->inode->i_sectors[block_idx];
        block_idx++;
    }
    block_idx = 0;

    if(pdir->inode->i_sectors[12] != 0){        // 含有一级间接块
        ide_read(part->my_disk, pdir->inode->i_sectors[12], all_blocks + 12, 1);
    }
    // all_blocks 获取到所有Inode文件或者目录所对应的扇区地址

    // 写目录项的时候保证目录项不跨扇区
    uint8_t* buf = (uint8_t*)sys_malloc(SECTOR_SIZE);
    struct dir_entry* p_de = (struct dir_entry*)buf;
    // p_de 为指向目录项的指针，值为buf起始地址
    uint32_t dir_entry_size = part->sb->dir_entry_size;
    uint32_t dir_entry_cnt = SECTOR_SIZE / dir_entry_size;
    // 1扇区内可容纳的目录项个数

    // 开始在所有块中查找目录项
    while(block_idx < block_cnt){
        // 块地址为0时表示该块中无数据，继续在其他块中找
        if(all_blocks[block_idx] == 0){
            block_idx++;
            continue;
        }
        // 将扇区中的 对应块 读取到 buf 当中
        ide_read(part->my_disk, all_blocks[block_idx], buf, 1);

        uint32_t dir_entry_idx = 0;
        /* 遍历扇区中所有目录项 */
        while(dir_entry_idx < dir_entry_cnt){
            /* 判断文件名是否一致，若找到了，则直接复制整个目录项 */
            if(!strcmp(p_de->filename, name)){
                memcpy(dir_e, p_de, dir_entry_size);
                sys_free(buf);
                sys_free(all_blocks);
                return true;
            }
            dir_entry_idx++;
            p_de++;
        }
        block_idx++;
        // p_de指向扇区内最后一个完整目录项，恢复到buf最开始
        p_de = (struct dir_entry*)buf;
        memset(buf, 0, SECTOR_SIZE);        // 清零
    }

    sys_free(buf);
    sys_free(all_blocks);
    return false;
}

/* 关闭目录，本质是关闭目录的inode并且释放目录的内存 */
void dir_close(struct dir* dir){
    /* 根目录不能关闭:
    1. 打开后不应该关闭，还需要再次open_root_dir()
    2. root_dir 所在的内存是低端1MB当中，不能free */
    if(dir == &root_dir)
        return;     
    inode_close(dir->inode);
    sys_free(dir);
}

/* 在内存中初始化目录项 p_de */
void create_dir_entry(char* filename, uint32_t inode_no, uint8_t file_type,
            struct dir_entry* p_de){

    ASSERT(strlen(filename) <= MAX_FILE_NAME_LEN);

    /* 初始化目录项 */
    memcpy(p_de->filename, filename, strlen(filename));
    p_de->i_no = inode_no;
    p_de->f_type = file_type;
}

/* 将目录项p_de写入父目录 parent_dir中并写入磁盘，io_buf由主调函数提供 */
bool sync_dir_entry(struct dir* parent_dir, struct dir_entry* p_de, void* io_buf){
    
    struct inode* dir_inode = parent_dir->inode;
    uint32_t dir_size = dir_inode->i_size;
    uint32_t dir_entry_size = dir_inode->i_size;

    // dir_size应该是dir_entry_size的整数倍
    ASSERT(dir_size % dir_entry_size == 0);
    
    uint32_t dir_entrys_per_sec = (512 / dir_entry_size);   // 每个扇区最大的目录项数目
    int32_t block_lba = -1;
    /* 将该目录的所有扇区地址 存入all_blocks当中 */
    uint8_t block_idx = 0;
    int32_t all_blocks[140] = {0};      // 保存目录所有的块

    while(block_idx < 12){
        all_blocks[block_idx] = parent_dir->inode->i_sectors[block_idx];
        block_idx++;
    }
    // 遍历目录项
    struct dir_entry* dir_e = (struct dir_entry*)io_buf;
    int32_t block_bitmap_idx = -1;
    // 一共140块
    block_idx = 0;
    while(block_idx < 140){
        
        block_bitmap_idx = -1;
        if(all_blocks[block_idx] == 0){     // 没有对应内存块，需要分配
            // 从当前分区获取一个块
            block_lba = block_bitmap_alloc(cur_partition);
            if(block_lba == -1){
                printk("alloc block bitmap for sync_dir_entry failed\n");
                return false;
            }
            // 分配后同步一次
            block_bitmap_idx = block_lba - cur_partition->sb->data_start_lba;
            ASSERT(block_bitmap_idx != -1);
            bitmap_sync(cur_partition, block_idx, BLOCK_BITMAP);

            block_bitmap_idx = -1;
            if(block_idx < 12){     // 直接块,直接保存到inode当中
                dir_inode->i_sectors[block_idx] = block_lba;
                all_blocks[block_idx] = block_lba;
            }else if(block_idx == 12){  // 一级间接块
                // 分配间接块表
                dir_inode->i_sectors[12] = block_lba;
                block_bitmap_idx = -1;
                block_lba = block_bitmap_alloc(cur_partition);
                if(block_lba == -1){        // 分配失败,还原设置，输出错误信息
                    block_bitmap_idx = dir_inode->i_sectors[12]  \
                                - cur_partition->sb->data_start_lba;
                    bitmap_set(&cur_partition->block_bitmap, block_idx, 0);
                    dir_inode->i_sectors[12] = 0;
                    printk("alloc block bitmap for sync_dir_entry failed\n");
                    return false;
                }

                // 再分配一个块，在间接块中作为第0个块进行存储
                block_bitmap_idx = block_lba - cur_partition->sb->data_start_lba;
                ASSERT(block_bitmap_idx != -1);
                bitmap_sync(cur_partition, block_bitmap_idx, BLOCK_BITMAP);

                all_blocks[12] = block_lba;
                // 写入到间接块表当中, 直接写入到对应的扇区当中
                ide_write(cur_partition->my_disk, dir_inode->i_sectors[12], all_blocks + 12, 1);
            }else{      // 间接块未分配
                all_blocks[block_idx] = block_lba;
                // 把新分配的第(block_idx - 12)个间接块地址写入一级间接块表
                ide_write(cur_partition->my_disk, dir_inode->i_sectors[12], all_blocks + 12, 1);
            }
            // 写入目录项
            memset(io_buf, 0, 512);
            memcpy(io_buf, p_de, dir_entry_size);
            ide_write(cur_partition->my_disk, all_blocks[block_idx], io_buf, 1);
            dir_inode->i_size += dir_entry_size;
            return true;
        }
        
        // 第block_idx块已经存在,读入对应的目录项
        ide_read(cur_partition->my_disk, all_blocks[block_idx], io_buf, 1);
        // 查找空余的目录
        uint8_t dir_entry_idx = 0;
        while(dir_entry_idx < dir_entrys_per_sec){
            // FT_UNKNOWN 是0，初始值就是该值
            if((dir_e + dir_entry_idx)->f_type == FT_UNKNOWN){
                memcpy(dir_e + dir_entry_idx, p_de, dir_entry_size);
                ide_write(cur_partition->my_disk, all_blocks[block_idx], io_buf, 1);
                dir_inode->i_size += dir_entry_size;
                return true;
            }
            dir_entry_idx++;
        }
        block_idx++;
    }
    printk("directory is full!\n");
    return false;
}

/* 把分区part目录pdir中编号为inode_no的目录项删除 */
bool delete_dir_entry(struct partition* part, struct dir* pdir, uint32_t inode_no, void* io_buf){
    struct inode* dir_inode = pdir->inode;
    uint32_t block_idx = 0, all_blocks[140] = {0};

    /* 收集目录全部地址 */
    while(block_idx < 12){
        all_blocks[block_idx] = dir_inode->i_sectors[block_idx];
        block_idx++;
    }
    if(dir_inode->i_sectors[12]){
        ide_read(part->my_disk, dir_inode->i_sectors[12], all_blocks + 12, 1);
    }

    /* 目录项在存储时保证不会跨扇区 */
    uint32_t dir_entry_size = part->sb->dir_entry_size;
    uint32_t dir_entrys_per_sec = (SECTOR_SIZE / dir_entry_size);
    // 每扇区最大的目录项数目
    struct dir_entry* dir_e = (struct dir_entry*)io_buf;
    struct dir_entry* dir_entry_found = NULL;
    uint8_t dir_entry_idx, dir_entry_cnt;
    bool is_dir_first_block = false;        // 目录的第一个块

    /* 遍历所有块，寻找目录项 */
    block_idx = 0;
    while(block_idx < 140){
        is_dir_first_block = false;
        if(all_blocks[block_idx] == 0){
            block_idx++;
            continue;
        }
        dir_entry_idx = dir_entry_cnt = 0;
        memset(io_buf, 0, SECTOR_SIZE);
        /* 读取扇区，获得目录项 */
        ide_read(part->my_disk, all_blocks[block_idx], io_buf, 1);

        // 遍历目录项
        while(dir_entry_idx < dir_entrys_per_sec){
            if((dir_e + dir_entry_idx)->f_type != FT_UNKNOWN){

                if(!strcmp((dir_e + dir_entry_idx)->filename, ".")){
                    is_dir_first_block = true;
                }else if(strcmp((dir_e + dir_entry_idx)->filename, ".") &&
                        strcmp((dir_e + dir_entry_idx)->filename, "..")){
                            dir_entry_cnt++;
                            // 统计扇区内的目录项个数，判断是否在删除后回收该扇区
                            if((dir_e + dir_entry_idx)->i_no == inode_no){
                            // 如果找到此i结点，就将其记录在dir_entry_found 
                                ASSERT(dir_entry_idx == NULL);
                                dir_entry_found = dir_e + dir_entry_idx;
                            }
                        }
            }
            dir_entry_idx++;
        }
        // 此扇区没找到，就下个扇区继续找
        if(dir_entry_found == NULL){
            block_idx++;
            continue;
        }
    // 找到目录项之后，清除该目录项并判断是否回收扇区，随后退出循环直接返回
        ASSERT(dir_entry_cnt >= 1);

        if(dir_entry_cnt == 1 && !is_dir_first_block){  // 扇区中只有一个目录项和第一个目录项，直接回收整个扇区
            /* 在位图中回收该块 */
            uint32_t block_bitmap_idx = all_blocks[block_idx] - part->sb->data_start_lba;
            bitmap_set(&part->block_bitmap, block_bitmap_idx, 0);
            bitmap_sync(cur_partition, block_bitmap_idx, BLOCK_BITMAP);

            /* 将块地址从数组i_sectors或索引表中去掉 */
            if(block_idx < 12){
                dir_inode->i_sectors[block_idx] = 0;
            }else{          // 位于间接块当中
                uint32_t indirect_blocks = 0;       // 间接块个数
                uint32_t indirect_block_idx = 12;   // 块地址索引
                while(indirect_block_idx < 140){
                    if(all_blocks[indirect_block_idx] != 0){
                        indirect_blocks++;
                    }
                }
                ASSERT(indirect_blocks >= 1);   // 包括当前的间接块

                if(indirect_blocks > 1){        // 有多个间接块
                    // 间接索引表所在的块回收
                    all_blocks[block_idx] = 0;
                    // 重新写入间接索引表
                    ide_write(part->my_disk, dir_inode->i_sectors[12], all_blocks + 12, 1);
                }else{  // 只有这一个间接块
                    block_bitmap_idx = dir_inode->i_sectors[12] - part->sb->data_start_lba;
                    bitmap_set(&part->block_bitmap, block_bitmap_idx, 0);
                    bitmap_sync(cur_partition, block_bitmap_idx, BLOCK_BITMAP);

                    dir_inode->i_sectors[12] = 0; // 将间接块表清零
                }
            }
        }else{      // 仅仅将该目录项清空
            memset(dir_entry_found, 0, dir_entry_size);
            ide_write(part->my_disk, all_blocks[block_idx], io_buf, 1);
        }
        // 更新i结点信息并同步到硬盘
        ASSERT(dir_inode->i_size >= dir_entry_size);
        dir_inode->i_size -= dir_entry_size;
        memset(io_buf, 0, SECTOR_SIZE * 2);
        inode_sync(part, dir_inode, io_buf);

        return true;
    }
// 没找到对应的目录项
    return false;
}