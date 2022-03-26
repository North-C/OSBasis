#include "fs.h"
#include "stdint.h"
#include "inode.h"
#include "ide.h"
#include "super_block.h"
#include "dir.h"
#include "../kernel/memory.h"
#include "stdio-kernel.h"
#include "string.h"
#include "debug.h"
#include "file.h"
#include "console.h"


struct partition* cur_partition;            // 默认情况下操作的分区

/* 挂载分区，从分区链表找到part_name分区，并将指针赋值给cur_part */
static bool mount_partition(struct list_elem* pelem, int arg){
    char* part_name = (char*) arg;
    struct partition* part = elem2entry(pelem, struct partition, part_tag);

    if(!strcmp(part->name, part_name)){
        cur_partition = part;
        struct disk* hd = cur_partition->my_disk;

/* sb_buf用来存储从硬盘上读入的超级块 */
        struct super_block* sb_buf = (struct super_block*)sys_malloc(SECTOR_SIZE);
        cur_partition->sb = (struct super_block*) sys_malloc(sizeof(struct super_block));
        if(cur_partition->sb == NULL){
            PANIC("alloc memory failed!");
        }

/* 读取超级块 */
        memset(sb_buf, 0, SECTOR_SIZE);
        ide_read(hd, cur_partition->start_lba+1, sb_buf, 1);

        // 复制到当前分区的超级块
        memcpy(cur_partition->sb, sb_buf, sizeof(struct super_block));


/* 将硬盘上的块位图读入到内存 */
        cur_partition->block_bitmap.bits = (uint8_t*) sys_malloc(sb_buf->block_bitmap_sects * SECTOR_SIZE);

        if(cur_partition->block_bitmap.bits == NULL){
            PANIC("alloc memory failed");
        }
        cur_partition->block_bitmap.btmp_bytes_len = sb_buf->block_bitmap_sects * SECTOR_SIZE;

        // 从硬盘上读入块位图的分区的block_bitmap.bits
        ide_read(hd, sb_buf->block_bitmap_lba, cur_partition->block_bitmap.bits, sb_buf->block_bitmap_sects);

 /* 将硬盘上的inode位图读入到内存 */
        cur_partition->inode_bitmap.bits = (uint8_t*)sys_malloc(sb_buf->inode_bitmap_sects * SECTOR_SIZE);

        if(cur_partition->inode_bitmap.bits == NULL){
            PANIC("alloc memory failed");
        }
        cur_partition->inode_bitmap.btmp_bytes_len = sb_buf->inode_bitmap_sects * SECTOR_SIZE;

        // 从硬盘上读入inode位图到分区的inode_bitmap.bits
        ide_read(hd, sb_buf->inode_bitmap_lba, cur_partition->inode_bitmap.bits, sb_buf->inode_bitmap_sects );

        list_init(&cur_partition->open_inodes);
        printk("mount %s done!\n", part->name);

        return true;        // 迎合主调函数 list_traversal ，让其停止遍历
    }
    return false;
}

/* 格式化分区，即初始化分区的元信息，创建文件系统 */
static void partition_format(struct partition* part){
    /* 1. blocks-bitmap_init */
    uint32_t boot_sector_sects = 1;
    uint32_t super_block_sects = 1;
    uint32_t inode_bitmap_sects = DIV_ROUND_UP(MAX_FILES_PER_PART, BITS_PER_SECTOR);

    // 结点位图占用分区中的扇区数
    uint32_t inode_table_sects = DIV_ROUND_UP(sizeof(struct inode) * MAX_FILES_PER_PART, SECTOR_SIZE);
    uint32_t used_sects = boot_sector_sects + super_block_sects + inode_bitmap_sects + inode_table_sects;
    uint32_t free_sects = part->sec_cnt - used_sects;


    // 简单处理块位图占据的扇区数
    uint32_t block_bitmap_sects = DIV_ROUND_UP(free_sects, BITS_PER_SECTOR);
    // 位图中位的长度，也是磁盘可用块的数量
    uint32_t block_bitmap_bit_len = free_sects - block_bitmap_sects;
    block_bitmap_sects = DIV_ROUND_UP(block_bitmap_bit_len, BITS_PER_SECTOR);

    /* 2. 初始化 super_bock */
    struct super_block sb;
    sb.magic = 0x19590318;
    sb.sec_cnt = part->sec_cnt;
    sb.inode_cnt = MAX_FILES_PER_PART;
    sb.part_lba_base = part->start_lba;

    // 第二个扇区开始，第0块是引导块，第1块是super block
    sb.block_bitmap_lba = sb.part_lba_base + 2;
    sb.block_bitmap_sects = block_bitmap_sects;

    // 接着block_bitmap之后就是 inode_bitmap_lba
    sb.inode_bitmap_lba = sb.block_bitmap_lba + sb.block_bitmap_sects;
    sb.inode_bitmap_sects = inode_bitmap_sects;

    sb.inode_table_lba = sb.inode_bitmap_lba + sb.inode_bitmap_sects;
    sb.inode_table_sects = inode_table_sects;

    sb.data_start_lba = sb.inode_table_lba + sb.inode_table_sects;
    sb.root_inode_no = 0;
    sb.dir_entry_size = sizeof(struct dir_entry);

    printk("%s info: \n", part->name);
    printk("   magic:0x%x\n   part_lba_base:0x%x\n   all_sectors:0x%x\n   inode_cnt:0x%x\n   block_bitmap_lba:0x%x\n   block_bitmap_sectors:0x%x\n   inode_bitmap_lba:0x%x\n   inode_bitmap_sectors:0x%x\n   inode_table_lba:0x%x\n   inode_table_sectors:0x%x\n   data_start_lba:0x%x\n", sb.magic, sb.part_lba_base, sb.sec_cnt, sb.inode_cnt, sb.block_bitmap_lba, sb.block_bitmap_sects, sb.inode_bitmap_lba, sb.inode_bitmap_sects, sb.inode_table_lba, sb.inode_table_sects, sb.data_start_lba);

    
    /* 3. 将超级块写入第一个扇区 */
    struct disk* hd = part->my_disk;
    ide_write(hd, part->start_lba + 1, &sb, 1);
    printk("super_block_lba: 0x%x\n", part->start_lba + 1);

    // 分配存储缓冲区，设定其大小为---最大数据量的元信息
    uint32_t buf_size = (sb.block_bitmap_sects >= sb.inode_bitmap_sects ? \
                    sb.block_bitmap_sects : sb.inode_bitmap_sects);

    buf_size = ((buf_size >= sb.inode_table_sects )? buf_size : sb.inode_table_sects) * SECTOR_SIZE; 
    uint8_t* buf = (uint8_t*) sys_malloc(buf_size);

    /* 4. 将块位图初始化并写入sb.block_bitmap_lba */
    // 初始化块位图 block_bitmap
    buf[0] |= 0x01;             // 第0块留给根目录，位图中先占位
    uint32_t block_bitmap_last_byte = block_bitmap_bit_len / 8;
    uint8_t block_bitmap_last_bit = block_bitmap_bit_len % 8 ;
    // 最后一个扇区中，不足一扇区的其余部分
    uint32_t last_size = SECTOR_SIZE - (block_bitmap_last_byte % SECTOR_SIZE);
    
    // 将末尾扇区的全部字节都占为己用，全部置位1
    memset(&buf[block_bitmap_last_byte], 0xff, last_size);

    // 再将上一步中覆盖的最后一个字节内的有效位重新置位0
    uint8_t bit_idx = 0;
    while(bit_idx <= block_bitmap_last_bit){
        buf[block_bitmap_last_byte] &= ~(1 << bit_idx++);
    }
    // 进行写入
    ide_write(hd, sb.block_bitmap_lba, buf, sb.block_bitmap_sects);

    /* 5. 初始化inode位图并写入sb.inode_bitmap.lba */
    // 清空缓冲区
    memset(buf, 0, buf_size);
    buf[0] |= 0x1;          // 分配第0个inode给根目录
    
    // inode_table中共4096个inode，占用1扇区，inode_bitmap占用1扇区
    // inode_bitmap所在的扇区中没有多余的无效位 
    ide_write(hd, sb.inode_bitmap_lba, buf, sb.inode_bitmap_sects);

    /* 6. 将inode数字初始化并写入sb.inode_table_lba */
    memset(buf, 0, buf_size);       // 清空缓冲区 buf
    
    struct inode* i =(struct inode*) buf;
    i->i_size = sb.dir_entry_size * 2;          // . 和 ..
    i->i_no = 0;            // 根目录占 inode 数组第0个 inode
    i->i_sectors[0] = sb.data_start_lba;
    ide_write(hd, sb.inode_table_lba, buf, sb.inode_table_sects);
    
    /* 7. 将根目录写入 sb.data_start_lba */
    memset(buf, 0, buf_size);
    struct dir_entry* p_de = (struct dir_entry*)buf;

    // 初始化当前目录 “.”
    memcpy(p_de->filename, ".", 1);
    p_de->i_no = 0;
    p_de->f_type = FT_DIRECTORY;
    p_de++;

    // 初始化当前目录父目录 .. 
    memcpy(p_de->filename, "..", 2);
    p_de->i_no = 0;
    p_de->f_type = FT_DIRECTORY;
    // 已经分配根目录，里面是根目录的目录项
    ide_write(hd, sb.data_start_lba, buf, 1);

    printk("    root_dir_lba:0x%x\n", sb.data_start_lba);
    printk("%s format done\n", part->name);
    sys_free(buf);
}

/* 在磁盘上搜索文件系统，若没有则格式化分区，创建文件系统 */
void filesys_init(){
    uint8_t channel_no= 0, dev_no, part_idx= 0;
    
    /* sb_buf 用来存储从硬盘上读入的超级块 */
    struct super_block* sb_buf = (struct super_block*)sys_malloc(SECTOR_SIZE);
    if(sb_buf == NULL){
        PANIC("alloc memory failed!");
    }
    printk("searching filesystem......\n");
    while(channel_no < channel_cnt){
        dev_no = 0;
        while (dev_no < 2)
        {
            if(dev_no == 0){    // 越过hd60M.img
                dev_no++;       // 下一个磁盘hd80M.img
                continue;
            }
            struct disk* hd = &channels[channel_no].devices[dev_no];
            struct partition* part = hd->primary_partition;
            while(part_idx < 12){       // 4个主分区 + 8个逻辑分区 ，最多12个分区
                if(part_idx == 4){      // 开始处理逻辑分区
                    part = hd->logical_partition;
                }

                if(part->sec_cnt != 0){     // 分区存在
                    memset(sb_buf, 0, SECTOR_SIZE);

                    ide_read(hd, part->start_lba + 1, sb_buf, 1);
                   
                    if(sb_buf->magic == 0x19590318){
                        printk("%s has filesystem\n", part->name);
                        
                        printk("%s info: \n", part->name);
                        printk("   magic:0x%x\n   part_lba_base:0x%x\n   all_sectors:0x%x\n   inode_cnt:0x%x\n   block_bitmap_lba:0x%x\n  \
                            block_bitmap_sectors:0x%x\n   inode_bitmap_lba:0x%x\n   inode_bitmap_sectors:0x%x\n   inode_table_lba:0x%x\n   inode_table_sectors:0x%x\n   data_start_lba:0x%x\n", \
                            cur_partition->sb->magic, cur_partition->sb->part_lba_base, cur_partition->sb->sec_cnt, cur_partition->sb->inode_cnt, cur_partition->sb->block_bitmap_lba, cur_partition->sb->block_bitmap_sects, cur_partition->sb->inode_bitmap_lba, \
                            cur_partition->sb->inode_bitmap_sects, cur_partition->sb->inode_table_lba, cur_partition->sb->inode_table_sects, cur_partition->sb->data_start_lba);
                    }else{
                        printk("formatting %s's partition %s...... \n", hd->name, part->name);
                        partition_format(part);
                    }
                }
                part_idx++;
                part++;         // 下一分区
            }
            dev_no++;           // 下一磁盘
        }
        channel_no++;       // 下一个通道   
    }
    sys_free(sb_buf);
    // 确认默认操作的分区
    char default_part[8] = "sdb1";
    // 挂载分区
    list_traversal(&partition_list, mount_partition, (int)default_part);

    /* 将当前分区的根目录打开 */
     open_root_dir(cur_partition);

     /* 初始化文件表 */
     uint32_t fd_idx = 0;
     while(fd_idx < MAX_FILE_OPEN){
         file_table[fd_idx++].fd_inode = NULL;
     }
}

/* 解析最上层路径，并保存到name_store当中 */
static char* path_parse(char* pathname, char* name_store){
    if(pathname[0] == '/'){     // 根目录不需要单独解析
        /* 路径中出现1个或多个连续的字符 / ，将其跳过*/
        while(*(++pathname)=='/');
    }
    /* 开始一般的路径解析 */
    while(*pathname !='/' && *pathname != 0){
        *name_store++ = *pathname++;
    }
    if(pathname[0] == 0){       // 整体为空,返回NULL
        return NULL;
    }
    return pathname;
}

/* 返回路径深度 */
int32_t path_depth_cnt(char* pathname){
    ASSERT(pathname != NULL);
    char* p = pathname;
    char name[MAX_FILE_NAME_LEN];

    uint32_t depth = 0;
    // 解析路径，拆分各级名称至name，并返回未解析的路径到 p
    p = path_parse(p, name);
    while(name[0]){     // 是否为结束符号
        depth++;
        memset(name, 0, MAX_FILE_NAME_LEN);
        if(p){
            p = path_parse(p, name);
        }
    }
    return depth;
}

/* 检索文件,找到则返回inode编号，否则返回-1 */
static int search_file(const char* pathname, struct path_search_record* searched_record){
    /* 判断是否为根目录 */
    if(!strcmp(pathname, "/") || !strcmp(pathname, "/.") || !strcmp(pathname, "/..")){
        searched_record->searched_path[0] = 0;
        searched_record->parent_dir = &root_dir;
        searched_record->file_type = FT_DIRECTORY;
        return 0;
    }

    // 保证文件名正常
    uint32_t path_len = strlen(pathname);
    ASSERT(pathname[0] == '/' && path_len < MAX_PATH_LEN && path_len > 1);
    
    char* sub_path = (char*) pathname;       // 转换为可变的
    struct dir* parent_dir = &root_dir;
    struct dir_entry dir_e;

    char name[MAX_FILE_NAME_LEN] = {0};     // 记录路径解析出来的各级名称

    searched_record->parent_dir = parent_dir;
    searched_record->file_type = FT_UNKNOWN;
    uint32_t parent_inode_no = 0;            // 父目录的inode号

    // 解析路径，开始查找
    sub_path = path_parse(sub_path, name);
    while(name[0]){

        ASSERT(strlen(searched_record->searched_path) < 512);
        // 父目录
        strcat(searched_record->searched_path, "/");
        strcat(searched_record->searched_path, name);

        if(search_dir_entry(cur_partition, parent_dir, name, &dir_e)){
            memset(name, 0, MAX_FILE_NAME_LEN);
            // 继续解析下去
            if(sub_path){
                sub_path = path_parse(sub_path, name);
            }

            // 为目录
            if(FT_DIRECTORY == dir_e.f_type){
                parent_inode_no = parent_dir->inode->i_no;      // 记录父目录的inode结点
                dir_close(parent_dir);
                searched_record->parent_dir = dir_open(cur_partition, dir_e.i_no);  
                parent_dir = searched_record->parent_dir;
                continue;
            }
            // 为常规文件
            else if(FT_REGULAR == dir_e.f_type){
                searched_record->file_type = FT_REGULAR;
                return dir_e.i_no;
            }

        }else{      // 找不到，返回-1，同时不关闭parent_dir，以便后续直接在此新建
            return -1;
        }
    }

    // 到这里必然是查找了完整的路径,且不是常规文件，只有同名目录存在
    dir_close(searched_record->parent_dir);

    // 保存直接父目录
    searched_record->parent_dir = dir_open(cur_partition, parent_inode_no);
    searched_record->file_type = FT_DIRECTORY;      
    return dir_e.i_no;         
}

/* 打开名为pathname的文件，返回文件描述符，失败则返回 -1  */
int32_t sys_open(const char* pathname, uint8_t flags){
    if(pathname[strlen(pathname) - 1] == '/'){
        printk("can't open a directory %s \n", pathname);
        return -1;
    }
    ASSERT(flags <= 7);
    int32_t fd = -1;    // 默认找不到路径

    struct path_search_record searched_record;
    memset(&searched_record, 0, sizeof(struct path_search_record));
    // 记录目录深度
    uint32_t pathname_depth = path_depth_cnt((char*)pathname);

    // 先检查文件是否存在
    int inode_no = search_file(pathname, &searched_record);
    bool found = inode_no != -1 ? true: false;

    if(searched_record.file_type == FT_DIRECTORY){
        printk("can't open a directory with open(), use opendir() to instead\n");
        dir_close(searched_record.parent_dir);
        return -1;
    }

    uint32_t path_searched_depth = path_depth_cnt(searched_record.searched_path);

    // 判断是否访问完了整个pathname的路径
    if(pathname_depth != path_searched_depth){
        printk("cannot access %s : Not a directory, subpath %s is't exist\n", \
                pathname, searched_record.searched_path);
        dir_close(searched_record.parent_dir);
        return -1;
    }

    // 如果在最后一个路径上没找到，且不进行文件创建，则直接返回 -1 
    if(!found && !(flags & O_CREAT)){
        printk("in path %s, file %s is't exist\n", \
                searched_record.searched_path, (strrchr(searched_record.searched_path, '/') + 1));
        dir_close(searched_record.parent_dir);
        return -1;
    }else if(found && flags & O_CREAT){        // 已经存在了，不需要重复创建
        printk("%s has already exist!\n", pathname);
        dir_close(searched_record.parent_dir);
        return -1;
    }

    switch(flags & O_CREAT){
        case O_CREAT:
            printk("creating file\n");
            fd = file_create(searched_record.parent_dir, (strrchr(pathname, '/') + 1), flags);
            dir_close(searched_record.parent_dir);
            // 其余为已打开文件
            break;
        default:
            fd = file_open(inode_no, flags);
    }
    // 此fd指的是任务pcb->fd_table数组的下标
    return fd;
}

/* 将文件描述符转化为文件表的下标 */
static uint32_t fd_local2global(uint32_t local_fd){
    struct task_struct* cur = running_thread();
    int32_t global_fd = cur->fd_table[local_fd];
    ASSERT(global_fd >= 0 && global_fd < MAX_FILE_OPEN);
    return (uint32_t)global_fd;
}

/* 关闭文件描述符fd指向的文件，成功返回0，失败返回-1 */
int32_t sys_close(int32_t fd){
    int32_t ret = -1;
    if(fd > 2){
        uint32_t _fd = fd_local2global(fd);
        ret = file_close(&file_table[_fd]);
        running_thread()->fd_table[fd] = -1;        // 可用
    }
    return ret;
}

/* 将buf中连续count个字节写入文件描述符fd, 成功则返回写入的字节数，失败返回-1 */
int32_t sys_write(int32_t fd, const void* buf, uint32_t count){
    if(fd < 0){
        printk("sys_write: fd error\n");
        return -1;
    }
    if (fd == stdout_no){
        char tmp_buf[1024] = {0};
        memcpy(tmp_buf, buf, count);
        console_put_str(tmp_buf);
        return count;
    }
    uint32_t _fd = fd_local2global(fd);     // 转换为全局的文件描述符
    struct file* wr_file = &file_table[_fd];
    // 判断读写权限
    if(wr_file->fd_flag & O_WRONLY || wr_file->fd_flag & O_RDWR){
        uint32_t bytes_written = file_write(wr_file, buf, count);
        return bytes_written;
    }else{
        console_put_str("sys_write: not allowed to write file \
                without flag O_RDWR or O_WRONLY\n");
        return -1;
    }
}