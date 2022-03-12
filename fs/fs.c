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
}