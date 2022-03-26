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
    int32_t block_idx = bitmap_scan(&part->block_bitmap, 1);
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

/* 编号inode_no的inode对应的文件，若成功则返回文件描述符，否则返回 -1 */
int32_t file_open(uint32_t inode_no, uint8_t flag){

    int fd_idx = get_free_slot_in_global();
    if(fd_idx == -1){
        printk("exceed max open files");
        return -1;
    }
    file_table[fd_idx].fd_inode = inode_open(cur_partition, inode_no);
    file_table[fd_idx].fd_pos = 0;

    // 要将fd_pos还原为0,即让文件内的指针指向开头
    file_table[fd_idx].fd_flag = flag;
    bool* write_deny = &file_table[fd_idx].fd_inode->write_deny;


    if(flag & O_WRONLY || flag & O_RDWR){
        // 关于写文件，判断是否由其他进程在写文件
        enum intr_status old_status = disable_intr();       // 先关闭中断
        if(!(*write_deny)){         // 没有其他进程使用
            *write_deny = true;
            set_intr_status(old_status);    
        }else{
            set_intr_status(old_status);
            printk("file can't be written now, try again later\n");
            return -1;
        }
    }
    // 返回进程内的fd_idx
    return pcb_fd_install(fd_idx);
}

/* 关闭文件 */
int32_t file_close(struct file* file){
    if(file==NULL){
        return -1;
    }
    file->fd_inode->write_deny = false;
    inode_close(file->fd_inode);
    file->fd_inode = NULL;
    return 0;
}

// 将buf中的count个字节写入文件file，成功返回写入的字节数，失败返回-1
int32_t file_write(struct file* file, const void* buf, uint32_t count){
    if((file->fd_inode->i_size + count) > (BLOCK_SIZE * 140)){
        // 文件目前最大支持 512*140 字节，一个块就是一个扇区（为了简单省事）
        printk("exceed max file size 71689 bytes, write file failed.\n");
        return -1;
    }
    // 用作缓冲区
    uint8_t* io_buf = sys_malloc(512);
    if(io_buf == NULL){
        printk("file_write: sys_malloc for io_buf failed\n");
        return -1;
    }
    int32_t block_lba = 0;     // 扇区的地址
    uint8_t block_bitmap_idx = 0;      // 存储块在位图中的索引
    // all_blocks 记录文件所有的块地址
    uint32_t* all_blocks =(uint32_t*) sys_malloc(BLOCK_SIZE + 48);
    if (all_blocks == NULL){
        printk("file_write: sys_malloc for all_block failed\n");
        return -1;
    }

    const uint8_t* src = buf;       // 指向buf中待写入的数据
    uint32_t bytes_written = 0;     // 记录写入文件的长度
    uint32_t size_left = count;  // 记录为写入数据大小

    // 是否是第一次写该文件, 如果是，先分配一个块
    if(file->fd_inode->i_sectors[0] == 0){
        block_lba = block_bitmap_alloc(cur_partition);
        if(block_lba==-1){
            printk("file_write: block_bitmap_alloc for block_lba failed\n");
            return -1;
        }
        file->fd_inode->i_sectors[0] = block_lba;

        /* 每分配一个块就将位图同步到硬盘 */
        block_bitmap_idx = block_lba - cur_partition->sb->data_start_lba;       // 为何减去data的开始地址
        ASSERT(block_bitmap_idx != 0);
        bitmap_sync(cur_partition, block_bitmap_idx, BLOCK_BITMAP);
    }

    uint32_t block_idx = 0;     // 硬盘块索引
    uint32_t indirect_block_table ;     // 间接块表
    // 写入count个字节前,该文件已经占有的硬盘块数
    uint32_t file_has_used_blocks = file->fd_inode->i_size / BLOCK_SIZE + 1; 
    // 存入后需要占用的
    uint32_t file_will_use_blocks = (file->fd_inode->i_size + count) / BLOCK_SIZE + 1;
    ASSERT(file_will_use_blocks <= 140);
    // 判断是否还需要额外分配扇区
    uint32_t add_blocks = file_will_use_blocks - file_has_used_blocks;

    
    if(add_blocks == 0){        // 不需要额外分配扇区
        
        if( file_will_use_blocks <= 12){    // 文件在12个块之内
            block_idx = file_has_used_blocks - 1;
            // 指向最后一个已有数据的扇区
            all_blocks[block_idx] = file->fd_inode->i_sectors[block_idx];

        }else{
            ASSERT(file->fd_inode->i_sectors[12] != 0);
            indirect_block_table = file->fd_inode->i_sectors[12];       // 间接块地址
            // 读取到
            ide_read(cur_partition->my_disk, indirect_block_table, all_blocks + 12, 1);
        }
    }else{  // 额外分配扇区
        // 分为三种情况
        // 一、 12个直接块够用
        if(file_will_use_blocks <= 12){
            block_idx = file_has_used_blocks - 1;
            ASSERT(file->fd_inode->i_sectors[block_idx] != 0);
            all_blocks[block_idx] = file->fd_inode->i_sectors[block_idx];

            block_idx = file_has_used_blocks;   // 第一个要分配的新扇区
            while(block_idx < file_will_use_blocks){
                block_lba = block_bitmap_alloc(cur_partition);
                if(block_lba == -1){        // 分配失败
                    printk("file_write: block_bitmap_alloc for situation 1 failed\n");
                    return -1;
                }

                ASSERT(file->fd_inode->i_sectors[block_idx] == 0);
                file->fd_inode->i_sectors[block_idx] = all_blocks[block_idx] = block_lba;

                block_bitmap_idx = block_lba - cur_partition->sb->data_start_lba;
                bitmap_sync(cur_partition, block_bitmap_idx, BLOCK_BITMAP);

                block_idx++;        // 下一个分配的新扇区
            }
        }else if(file_has_used_blocks <= 12 && file_will_use_blocks > 12){
            // 情况二，旧数据在12个直接块内，新数据使用到新数据块

            block_idx = file_has_used_blocks - 1;
            all_blocks[block_idx] = file->fd_inode->i_sectors[block_idx];

            // 创建一级间接块
            block_lba = block_bitmap_alloc(cur_partition);
            if (block_lba == -1){
                printk("file_write: block_bitmap_alloc for situation2 failed\n");
                return -1;
            }
            // 间接块未分配
            ASSERT(file->fd_inode->i_sectors[12] == 0);
            indirect_block_table = file->fd_inode->i_sectors[12] = block_lba;

            block_idx = file_has_used_blocks ;      // 第一个未使用的块

            while(block_idx < file_will_use_blocks){
                block_lba = block_bitmap_alloc(cur_partition);
                if (block_lba == -1){
                    printk("file_write: block_bitmap_alloc for situation 2 failed\n");
                    return -1;
                }

                if(block_idx < 12){ 
                    // 新创建的直接块存入all_blocks数组中
                    ASSERT(file->fd_inode->i_sectors[block_idx] == 0);
                    file->fd_inode->i_sectors[block_idx] = all_blocks[block_idx] = block_lba;

                }else{
                    // 间接块值写入到all_blocks数组
                    all_blocks[block_idx] = block_lba;
                }

                /* 每次分配一个块就将位图同步到硬盘 */
                block_bitmap_idx = block_lba - cur_partition->sb->data_start_lba;
                bitmap_sync(cur_partition, block_bitmap_idx, BLOCK_BITMAP);

                block_idx++;        // 下一个新扇区
            }
            // 同步一级间接块到硬盘
            ide_write(cur_partition->my_disk, indirect_block_table, all_blocks+12, 1);
        }else if(file_has_used_blocks > 12){
            //情况三，新数据块完全占据间接块
            ASSERT(file->fd_inode->i_sectors[12] != 0);
            // 一级间接表的地址
            indirect_block_table = file->fd_inode->i_sectors[12];
            // 间接块全部读入到all_blocks当前，获取到所有间接块的地址
            ide_read(cur_partition->my_disk, indirect_block_table, all_blocks + 12, 1);

            while(block_idx < file_will_use_blocks){
                block_lba = block_bitmap_alloc(cur_partition);
                if(block_lba == -1){
                    printk("file_write: block_bitmap_alloc for situation 2 failed\n");;
                    return -1;
                }
                all_blocks[block_idx++] = block_lba;

                /* 分配一个块就进行同步 */
                block_bitmap_idx = block_lba - cur_partition->sb->data_start_lba;
                bitmap_sync(cur_partition, block_bitmap_idx, BLOCK_BITMAP);
            }
            // 将间接表同步到硬盘
            ide_write(cur_partition->my_disk, indirect_block_table, all_blocks + 12, 1);
        }

    }
    
    uint32_t sec_lba;       // 索引扇区
    uint32_t sec_idx;       // 扇区地址
    uint32_t sec_off_bytes; // 扇区内字节偏移量
    uint32_t sec_left_bytes;    // 扇区内剩余字节量
    uint32_t chunk_size;        // 每次写入硬盘的数据块大小
    
    /* 所用到的块地址全部收集到了all_blocks当中，之后开始写数据 */
    bool first_write_block = true;       // 含义剩余空间的块标识
    file->fd_pos = file->fd_inode->i_size - 1;
    while(bytes_written < count){       
        memset(io_buf, 0, BLOCK_SIZE);
        sec_idx = file->fd_inode->i_size / BLOCK_SIZE;
        sec_lba = all_blocks[sec_idx];
        sec_off_bytes = file->fd_inode->i_size % BLOCK_SIZE;
        sec_left_bytes = BLOCK_SIZE - sec_off_bytes;

        /* 判断此次写入硬盘的数据大小 */
        chunk_size = size_left < sec_left_bytes ? size_left: sec_left_bytes;
        if(first_write_block){
            ide_read(cur_partition->my_disk, sec_lba, io_buf, 1);
            first_write_block = false;
        }
        memcpy(io_buf + sec_off_bytes, src, chunk_size);    // src指向buf参数中需要写入的数据
        ide_write(cur_partition->my_disk, sec_lba, io_buf, 1);
        printk("file write at lba 0x%x\n", sec_lba);    // 调试，完成后可去掉

        src += chunk_size;  // 下一个需要写入的新数据
        file->fd_inode->i_size += chunk_size;       // 更新文件大小
        file->fd_pos += chunk_size;
        bytes_written += chunk_size;
        size_left -= chunk_size;
    }

    inode_sync(cur_partition, file->fd_inode, io_buf);
    sys_free(all_blocks);
    sys_free(io_buf);
    return bytes_written;
}

/* 从文件file中读取count个字节写入buf, 返回读出的字节数，若到文件尾则返回-1 */
int32_t file_read(struct file* file, void* buf, uint32_t count){
    uint8_t* buf_dst = (uint8_t*)buf;
    uint32_t size = count, size_left = size;

    /* 将要读取的字节数超出了文件可读的剩余量 */
    if((file->fd_pos + count) > file->fd_inode->i_size){
        // 将文件的剩余量作为将要读取的字节数
        size = file->fd_inode->i_size - file->fd_pos;
        size_left = size;
        if(size == 0){
            return -1;
        }
    }
    // 用作硬盘读写缓冲区
    uint8_t* io_buf = (uint8_t *)sys_malloc(BLOCK_SIZE);
    if(io_buf == NULL){
        printk("file_read: sys_malloc for io_buf failed\n");
    }
   
    // 记录文件块地址
    uint32_t* all_blocks = (uint32_t*)sys_malloc(BLOCK_SIZE + 48);
    if(all_blocks==NULL){
        printk("file_read: sys_malloc for all_blocks failed\n");
        return -1;
    }

    uint32_t block_read_start_idx = file->fd_pos / BLOCK_SIZE;  // 数据所在块的起始地址
    uint32_t block_read_end_idx = (file->fd_pos + size)/ BLOCK_SIZE;       // 数据所在块的结束地址
    uint32_t read_blocks = block_read_end_idx - block_read_start_idx;   // 

    ASSERT(block_read_start_idx < 139 && block_read_end_idx <139);

    int32_t indirect_block_table;       // 间接块表地址
    uint32_t block_idx;             // 待读的块地址
    /* 构建all_blocks块地址数组*/
    
    /* 构建all_blocks块地址数组 */
    if(read_blocks == 0){       // 在同一个扇区当中
        ASSERT(block_read_end_idx == block_read_start_idx);
        if(block_read_end_idx < 12){        // 位于文件的直接块中
            block_idx = block_read_end_idx;
            all_blocks[block_idx] = file->fd_inode->i_sectors[block_idx];
        }else{  // 设计间接块表
            indirect_block_table = file->fd_inode->i_sectors[12];
            ide_read(cur_partition->my_disk, indirect_block_table, all_blocks+12, 1);
        }
    }else{      // 需要读取多个块
        // 第一种情况：全部位于直接块种
        if(block_read_end_idx < 12){
            block_idx = block_read_start_idx;
            while(block_idx <= block_read_end_idx){
                all_blocks[block_idx] = file->fd_inode->i_sectors[block_idx];
                block_idx++;
            }   
        }else if(block_read_start_idx < 12 && block_read_end_idx>=12){    
             // 第二种： 起始块位于直接块，结束块位于间接块
             block_idx = block_read_start_idx;
             while(block_idx < 12){
                all_blocks[block_idx] = file->fd_inode->i_sectors[block_idx];
                block_idx++;
             }
             ASSERT(file->fd_inode->i_sectors[12]!=0);      // 保证间接块已分配

             // 将间接块从硬盘读入到all_blocks当中
             indirect_block_table = file->fd_inode->i_sectors[12];
             ide_read(cur_partition->my_disk, indirect_block_table, all_blocks+12, 1);
        }else{      // 全部位于间接块中
            ASSERT(file->fd_inode->i_sectors[12]!=0);      // 保证间接块已分配
            indirect_block_table = file->fd_inode->i_sectors[12];
            ide_read(cur_partition->my_disk, indirect_block_table, all_blocks+12, 1);
        }
    }

    /* 所需块地址收集完毕，开始读取数据 */
    uint32_t sec_idx, sec_lba, sec_off_bytes, sec_left_bytes, chunk_size;
    uint32_t bytes_read = 0;        // 已读的数据，字节为单位
    while(bytes_read < size){
        sec_idx = file->fd_pos / BLOCK_SIZE;    // 文件中的第几个扇区
        sec_lba = all_blocks[sec_idx];
        sec_off_bytes = file->fd_pos % BLOCK_SIZE;      // 扇区中的偏移量
        sec_left_bytes = file->fd_inode->i_size - sec_off_bytes;    
        // 块大小，chunk_size保证不会读取多余的信息
        chunk_size = size_left < sec_left_bytes ? size_left: sec_left_bytes;
        
        memset(io_buf, 0, BLOCK_SIZE);
        ide_read(cur_partition->my_disk, sec_lba, io_buf, 1);
        memcpy(buf_dst, io_buf+sec_off_bytes, chunk_size);  //buf_dst指向buf
        buf_dst += chunk_size;
        file->fd_pos += chunk_size;
        bytes_read += chunk_size;
        size_left -= chunk_size;    
    }
    
    sys_free(all_blocks);
    sys_free(io_buf);
    return bytes_read;
}