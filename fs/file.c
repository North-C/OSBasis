#include "file.h"
#include "global.h"
#include "thread.h"
#include "ide.h"
#include "thread.h"
#include "fs.h"
#include "super_block.h"
#include "inode.h"
#include "stdio-kernel.h"
#include "interrupt.h"
#include "string.h"
#include "debug.h"

// 文件表
struct file file_table[MAX_FILE_OPEN];

/* 从文件表file_table中获取一个空闲位，成功返回下标，失败返回-1 */
int32_t get_free_slot_in_global(void){
    uint32_t fd_idx = 3;
    while(fd_idx < MAX_FILE_OPEN){
        if( file_table[fd_idx].fd_inode == NULL){
            break;
        }
        fd_idx++;
    }
    if(fd_idx == MAX_FILE_OPEN){
        printk("exceed max open files\n");
        return -1;
    }
    return fd_idx;
}

/* 将全局描述符下标安装到进程 或 线程自己的文件描述符数组fd_table,成功返回下标，失败返回-1 */
int32_t pcb_fd_install(int32_t global_fd_idx){
    struct task_struct* cur = running_thread();
    uint8_t local_fd_idx = 3; // 从第3个开始查找
    while(local_fd_idx < MAX_FILES_OPEN_PER_PROC){
        if(cur->fd_table[local_fd_idx] == -1){      // -1
            cur->fd_table[local_fd_idx] = global_fd_idx;    //安装文件描述符
            break;
        }
        local_fd_idx++;
    }
    if(local_fd_idx == MAX_FILES_OPEN_PER_PROC){
        printk("exceed max open files_open_proc\n");
        return -1;
    }
    return local_fd_idx;
}

/* 分配一个i结点，返回i节点号,失败返回-1 */
int32_t inode_bitmap_alloc(struct partition* part){
    // 从位图当中查找空闲结点
    int32_t bit_idx = bitmap_scan(&part->inode_bitmap, 1);
    if(bit_idx == -1){      // 查找失败
        return -1;
    }
    bitmap_set(&part->inode_bitmap, bit_idx, 1);
    return bit_idx;
}

/* 分配一个扇区，返回其扇区地址 */
int32_t block_bitmap_alloc(struct partition* part){
    uint32_t block_idx = bitmap_scan(&part->block_bitmap, 1);
    if(block_idx == -1){
        return -1;
    }

    bitmap_set(&part->block_bitmap, block_idx, 1);
    // 计算分配扇区的地址
    return (part->sb->data_start_lba + block_idx);
}

/* 将内存中bitmap第bit_idx位所在的512字节同步到硬盘,btmp表示位图种类 */
void bitmap_sync(struct partition* part, uint32_t bit_idx, uint8_t btmp){
    uint32_t off_sec = bit_idx / 4096;          // 本i结点索引相对于位图的扇区偏移量
    
    uint32_t off_size = off_sec * BLOCK_SIZE;   // 索引相对于位图的字节偏移量

    uint32_t sec_lba;   // 目的扇区的起始地址

    uint8_t* bitmap_off;        // 指向i结点对应的bitmap值,字节为单位

    // 同步到硬盘的位图只有两种
    switch(btmp){
        case INODE_BITMAP:
            sec_lba = part->sb->inode_bitmap_lba + off_sec;
            bitmap_off = part->inode_bitmap.bits + off_size;
            break;
        case BLOCK_BITMAP:
            sec_lba = part->sb->block_bitmap_lba + off_sec;
            bitmap_off = part->block_bitmap.bits + off_size;
            break;
    }
    // 写入到磁盘分区
    ide_write(part->my_disk, sec_lba, bitmap_off, 1);
}

// 文件创建,成功返回对应的inode编号，失败返回-1
int32_t file_create(struct dir* parent_dir, char* filename, uint8_t flag){
    /* 创建的顺序是：inode结点 文件描述符 文件目录项*/
    uint32_t rollback_step = 0;     // 用于回滚
  // 硬盘读写的公共缓冲区
    void* io_buf = sys_malloc(1024);
    if(io_buf == NULL){
        printk("in file_create: sys_malloc for io_buf failed.\n");
        return -1;
    }

    // 先获取inode编号
    int32_t new_inode_no = inode_bitmap_alloc(cur_partition);
    if(new_inode_no == -1){
        printk("alloc inode bitmap failed\n");
        return -1;
    }
    
    // 给新inode分配内存，并且初始化
    struct inode* new_file_inode = (struct inode *) sys_malloc(sizeof(struct inode));
    if(new_file_inode == NULL){
        printk("file_create: sys_malloc for inode failed.\n");
        rollback_step = 1;
        goto rollback;
    }
    inode_init(new_inode_no, new_file_inode);

    // 处理文件描述符,获取一个全局描述符表的空位
    int32_t fd_table_idx = get_free_slot_in_global();
    if(fd_table_idx == -1){
        printk("in file_create: get_free_lsot_in_global failed.\n");
        rollback_step = 2;
        goto rollback;
    }

    file_table[fd_table_idx].fd_inode = new_file_inode;
    file_table[fd_table_idx].fd_pos = 0;
    file_table[fd_table_idx].fd_flag = flag;
    file_table[fd_table_idx].fd_inode->write_deny = false;

    // 处理目录相关
    struct dir_entry new_dir_entry;
    memset(&new_dir_entry, 0, sizeof(struct dir_entry));
    create_dir_entry(filename, new_inode_no, FT_REGULAR, &new_dir_entry);
    // 将目录项写入目录，并且同步目录到磁盘当中
    if(!sync_dir_entry(parent_dir, &new_dir_entry, io_buf)){
        printk("in file_create: sync_dir_entry failed.\n");
        rollback_step = 3;
        goto rollback;
    }
    // 同步父目录的inode到硬盘中
    memset(io_buf, 0, 1024);
    inode_sync(cur_partition, parent_dir->inode, io_buf);

    // 同步inode到硬盘中
    memset(io_buf, 0, 1024);
    inode_sync(cur_partition, new_file_inode, io_buf);

    // 同步inode表到硬盘中
    bitmap_sync(cur_partition, fd_table_idx , INODE_BITMAP);

    // 将当前的inode添加到当前打开文件链表当中
    list_push(&cur_partition->open_inodes, &new_file_inode->inode_tag);

    new_file_inode->i_open_cnts = 1;
    sys_free(io_buf);
    // 将inode安装到进程的文件打开表
    return pcb_fd_install(fd_table_idx);

    /* 创建失败后，需要回滚 */
rollback:
    switch(rollback_step){
        case 3:
            memset( file_table+fd_table_idx, 0, sizeof(struct file));
        case 2:
            sys_free(new_file_inode);
        case 1:
            bitmap_set(&cur_partition->inode_bitmap,  new_inode_no, 0);
            break;
    }
    sys_free(io_buf);
    return -1;
}