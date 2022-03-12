#include "ide.h"
#include "interrupt.h"
#include "io.h"
#include "debug.h"
#include "timer.h"
#include "stdio.h"
#include "memory.h"
#include "list.h"
#include "stdio-kernel.h"
#include "string.h"

#define reg_data(channel)  (channel->port_base + 0)
#define reg_error(channel)  (channel->port_base + 1)
#define reg_sec_cnt(channel)  (channel->port_base + 2)
#define reg_lba_l(channel)  (channel->port_base + 3)
#define reg_lba_m(channel)  (channel->port_base + 4)
#define reg_lba_h(channel)  (channel->port_base + 5)
#define reg_dev(channel)  (channel->port_base + 6)          // 选择硬盘
#define reg_status(channel)  (channel->port_base + 7)
#define reg_cmd(channel)  (reg_status(channel))
#define reg_alt_status(channel) (channel->port_base + 0x206)
#define reg_ctl(channel)  reg_alt_status(channel)


/* reg_alt_status 寄存器的一些关键位 */
#define BIT_ALT_STAT_BSY  0x80 // 硬盘忙
#define BIT_ALT_STAT_DRDY  0x40  // 驱动器准备好了
#define BIT_ALT_STAT_DRQ  0x8   // 数据传输准备好了


/* device 寄存器的一些关键位 */
#define BIT_DEV_MBS   0xa0    // 第7位和第5位固定为1
#define BIT_DEV_LBA   0x40
#define BIT_DEV_DEV   0x10

/* 硬盘操作指令 */
#define CMD_IDENTIFY   0xec         // identify指令
#define CMD_READ_SECTOR  0x20       // 读扇区指令
#define CMD_WRITE_SECTOR  0x30    // 写扇区指令

/* 定义可读写的最大扇区数，调试用,避免出现扇区地址越界 */
#define max_lba  ((80 * 1024 * 1024 / 512) - 1)   // 80MB的硬盘

uint8_t channel_cnt;        // 按照硬盘数计算的通道数
struct ide_channel channels[2];     // 两个ide通道

/* 用于记录总扩展分区的起始lba，初始为0，partition_scan时以此为标记 */
int32_t ext_lba_base = 0;

uint8_t p_no = 0, l_no = 0;      // 用于记录硬盘主分区和逻辑分区的下标

struct list partition_list;         // 分区队列

/* 分区表项 结构体 */
struct partition_table_entry{
    uint8_t bootable;       // 是否可引导
    uint8_t start_head;     // 起始磁头号
    uint8_t start_sec;      // 起始扇区数
    uint8_t start_chs;      // 起始柱面号
    uint8_t fs_type;        // 分区类型,4个主分区，0x5表示扩展分区
    uint8_t end_head;       // 结束磁头号
    uint8_t end_sec;        // 结束扇区数
    uint8_t end_chs;        // 结束柱面号

    uint32_t start_lba;     // 起始扇区的lba地址
    uint32_t sec_cnt;       // 分区的扇区数目
}__attribute__ ((packed));

/* 引导扇区， mbr或ebr所在的扇区 */
struct boot_sector{
    uint8_t other[446];         // 引导代码
    struct partition_table_entry partition_table[4];    // 分区表项

    uint16_t signature;         // 扇区结尾标志
}__attribute__ ((packed));


/* 选择读写的硬盘 */
static void select_disk(struct disk* hd){
    uint8_t reg_device = BIT_DEV_MBS | BIT_DEV_LBA;
    if(hd->dev_no == 1){
        reg_device |= BIT_DEV_DEV;
    }
    outb(reg_dev(hd->my_channel), reg_device);   

}

/* 写入起始扇区地址和读写的扇区数 */
static void select_sector(struct disk* hd, uint32_t lba, uint8_t sec_cnt){
    ASSERT(lba < max_lba);
    outb(reg_sec_cnt(hd->my_channel), sec_cnt);

    outb(reg_lba_l(hd->my_channel), lba);
    outb(reg_lba_m(hd->my_channel), lba>>8);
    outb(reg_lba_h(hd->my_channel), lba>>16);
    
    // 因为部分LBA地址存在于dev当中，需要重新将device寄存器的值重新写入
    outb(reg_dev(hd->my_channel), BIT_DEV_MBS | BIT_DEV_LBA | ((hd->dev_no==1)? BIT_DEV_DEV :0) 
            | lba>>24);
}

/* 向通道channel发出命令cmd */
static void cmd_out(struct ide_channel* channel, uint8_t cmd){
    // 设置中断标记为 true ，便于中断处理程序的识别
    channel->expecting_intr = true;
    outb(reg_cmd(channel), cmd);    
}

/* 从硬盘 hd 读入 sec_cnt 个扇区到 buf */
static void read_from_sector(struct disk* hd, void* buf, uint8_t sec_cnt){
    uint32_t size_in_byte;
    if(sec_cnt == 0 ){
        // sec_cnt是8位长度，原值为256时，会将最高位1截断，使得值为0
        size_in_byte = 256 * 512;       
    }else{
        size_in_byte = sec_cnt * 512;
    }
    insw(reg_data(hd->my_channel), buf, size_in_byte / 2);  // 以 word 为单位
}

/* 将buf中sec_cnt扇区的数据写入硬盘hd */
static void write2sector(struct disk* hd, void* buf, uint8_t sec_cnt){
    uint32_t size_in_byte;
    if(sec_cnt == 0 ){
        // sec_cnt是8位长度，原值为256时，会将最高位1截断，使得值为0
        size_in_byte = 256 * 512;       
    }else{
        size_in_byte = sec_cnt * 512;
    }
    outsw(reg_data(hd->my_channel), buf, size_in_byte / 2);
}

/* 等待硬盘30s,返回是否准备好 */
static bool busy_wait(struct disk* hd){
    struct ide_channel* channel = hd->my_channel;
    int16_t time_limit = 30 * 1000;        // 毫秒为单位
    while(time_limit -= 10 >= 0){
        // 判断硬盘状态
        if( !(inb(reg_status(hd->my_channel)) & BIT_ALT_STAT_BSY )){
            // 硬盘驱动器是否准备好，是否能取数据
            return (inb(reg_status(hd->my_channel)) & BIT_ALT_STAT_DRQ);
        }else{
            mtime_sleep(10);
        }
    }
    return false;
}

/* 从硬盘中读取sec_cnt个扇区到 buf */
void ide_read(struct disk* hd, uint32_t lba, void* buf, uint32_t sec_cnt){
    ASSERT(sec_cnt > 0);    
    ASSERT(lba <= max_lba); 
    // 保持原子性
    lock_acquire(&hd->my_channel->lock);
    // 选取硬盘
    select_disk(hd);

    uint32_t sec_every;     // 每此读取多少个扇区
    uint32_t sec_done = 0;

    while(sec_done < sec_cnt){
        if( (sec_done + 256) <= sec_cnt){
            sec_every = 256;
        }else{
            sec_every = sec_cnt - sec_done;
        }
        // 输入 地址 和 读取的扇区数
        select_sector(hd, lba + sec_done, sec_every);
        // 向 cmd 当中输入 读取命令,此时硬盘开始运行
        cmd_out(hd->my_channel, CMD_READ_SECTOR);
        // 由于硬盘读取速度较慢，因此中间转让CPU使用权，使用主动转让的方式可能依旧需要等待
        // 使用thread_block来阻塞，完成读取后发送中断信号，等待中断处理程序唤醒
        sema_down(&hd->my_channel->disk_done);

        // 判断硬盘是否为 忙状态
        if(!busy_wait(hd)){
            char error[64];
            sprintf(error, "%s read sector %d failed!\n", hd->name, lba);
            PANIC(error);
        }
        // 从缓冲区中读取数据
        read_from_sector(hd, (void*)((uint32_t) buf + sec_done * 512) , sec_every);
        
        sec_done += sec_every;
    }
    lock_release(&hd->my_channel->lock);

}

void ide_write(struct disk* hd, uint32_t lba, void* buf, uint32_t sec_cnt){
    ASSERT(lba <= max_lba); 
    ASSERT(sec_cnt > 0); 
    // 保持原子性
    lock_acquire(&hd->my_channel->lock);
    // 选取硬盘
    select_disk(hd);

    uint32_t sec_every;     // 每此读取多少个扇区
    uint32_t sec_done = 0;

    while(sec_done < sec_cnt){
        if(sec_done + 256 <= sec_cnt){
            sec_every = 256;
        }else{
            sec_every = sec_cnt - sec_done;
        }
        // 输入 地址 和 读取的扇区数
        select_sector(hd, lba + sec_done, sec_every);
        // 向 cmd 当中输入 读取命令,此时硬盘开始运行
        cmd_out(hd->my_channel, CMD_WRITE_SECTOR);
       
        // 判断硬盘是否为 忙状态
        if(!busy_wait(hd)){
            char error[64];
            sprintf(error, "%s read sector %d failed!\n", hd->name, lba);
            PANIC(error);
        }
        // 从缓冲区中读取数据
        write2sector(hd, (void*)((uint32_t) buf + sec_done * 512) , sec_every);
         
        // 在硬盘响应期间阻塞自己
        sema_down(&hd->my_channel->disk_done);

        sec_done += sec_every;
    }
    lock_release(&hd->my_channel->lock);
}

/* 硬盘中断处理程序 */
void intr_hd_handler(uint8_t irq_no){
    ASSERT(irq_no == 0x2e || irq_no == 0x2f);

    uint8_t ch_no = irq_no - 0x2e;      // 通道号
    struct ide_channel* channel = &channels[ch_no];

    ASSERT(channel->irq_no == irq_no);
    // 每次硬盘都会申请锁，从而保证同步一致性
    if(channel->expecting_intr){
        channel->expecting_intr = false;    
        sema_up(&channel->disk_done);       // 信号量增加，释放锁
        // 读取状态寄存器使得硬盘控制器认为中断已经被处理,然后清除中断
        inb(reg_status(channel));
    }
}

/* 将dst中len个相邻字节交换位置后存入buf */
static void swap_pairs_bytes(const char* dst, char* buf, uint32_t len){
    uint32_t idx;
    for(idx = 0; idx < len; idx += 2){
        // 交换相邻字节的位置
        buf[idx + 1] = *dst++;
        buf[idx] = *dst++;
    }
    buf[idx] = '\0';
}

/* 执行identify指令，获取硬盘参数信息 */
static void identify_disk(struct disk* hd){
    char id_info[512];
    select_disk(hd);
    cmd_out(hd->my_channel, CMD_IDENTIFY);
    sema_down(&hd->my_channel->disk_done);
    
    if(!busy_wait(hd)){
        char error[64];
        sprintf(error, "%s identify sector failed!!!\n", hd->name);
        PANIC(error);
    }

    read_from_sector(hd, id_info, 1);

    char buf[64];
    uint8_t sn_start = 10*2, sn_len = 20, md_start = 27 * 2, md_len = 40;
    swap_pairs_bytes(&id_info[sn_start], buf, sn_len);
    printk(" disk %s info: \n   SN: %s\n", hd->name, buf);
    memset(buf, 0, sizeof(buf));
    swap_pairs_bytes(&id_info[sn_start], buf, sn_len);
    printk("   MODULE: %s\n", buf);
    uint32_t sectors = *(uint32_t*)&id_info[60 * 2];
    printk("   SECTORS: %d\n", sectors);
    printk("   CAPACITY: %dMB\n", sectors*512/1024/1024);

}

/* 扫描硬盘hd中地址为ext_lba的扇区中的所有分区 */
static void partition_scan(struct disk* hd, uint32_t ext_lba){
    struct boot_sector* bs = sys_malloc(sizeof(struct boot_sector));
    ide_read(hd, ext_lba, bs, 1);       // 读取引导扇区
    uint8_t part_num = 0;
    struct partition_table_entry* p = bs->partition_table;

    while(part_num++ < 4){
        if(p->fs_type == 0x5){      // 若为扩展分区
            if(ext_lba_base != 0){      
                // 每个子扩展分区都有一个分区表，需要递归扫描
                partition_scan(hd, p->start_lba + ext_lba_base );
            }else{      // 为0，表示第一次获取分区表
                ext_lba_base = p->start_lba;
                partition_scan(hd, p->start_lba );
            }
        }else if(p->fs_type != 0){             // 有效分区
            if(ext_lba == 0){           // 全是主分区
                hd->primary_partition[p_no].start_lba = ext_lba + p->start_lba;
                hd->primary_partition[p_no].sec_cnt = p->sec_cnt;
                hd->primary_partition[p_no].my_disk = hd;
                list_append(&partition_list, &hd->primary_partition[p_no].part_tag);
                sprintf(hd->primary_partition[p_no].name, "%s%d", hd->name, p_no+1);
                p_no++;
                ASSERT(p_no < 4);
            }else{                  // 
                hd->logical_partition[l_no].start_lba = ext_lba + p->start_lba;
                hd->logical_partition[l_no].sec_cnt = p->sec_cnt;
                hd->logical_partition[l_no].my_disk = hd;
                list_append(&partition_list, &hd->logical_partition[l_no].part_tag);
                sprintf(hd->logical_partition[l_no].name, "%s%d", hd->name, l_no+5);
                l_no++;
                if(l_no >= 8){
                    return;
                }
            }
        }
        p++;
    }
    sys_free(bs);
}

/* 打印分区信息 */
static bool partition_info(struct list_elem* pelem, int arg UNUSED){
    struct partition* part = elem2entry(pelem, struct partition, part_tag);
    printk("  %s start_lba:0x%x, sec_cnt:0x%x\n", part->name, part->start_lba, part->sec_cnt);            

    return false;
}

void ide_init(){
    printk("ide_init start\n");
    uint8_t hd_cnt = *((uint8_t*) (0x475));     // 在低端1MB以内，获取硬盘的数量
    ASSERT(hd_cnt > 0);
    list_init(&partition_list);         // 初始化分区列表
    channel_cnt = DIV_ROUND_UP(hd_cnt, 2);  // 通过硬盘数计算ide通道数

    struct ide_channel* channel;
    uint8_t channel_no = 0, dev_no = 0;

    /* 处理每个通道上的硬盘 */
    while(channel_no < channel_cnt){
        channel = &channels[channel_no];
        sprintf(channel->name, "ide%d", channel_no);

        switch(channel_no){
            case 0:
                channel->port_base = 0x1f0;   // ide0通道的起始端口
                channel->irq_no = 0x20 + 14;  // 从片的中断引脚14，0x20端口
                break;

            case 1:
                channel->port_base = 0x170;     // ide1通道的起始端口
                channel->irq_no = 0x20 + 15;     
                break;
        }
        channel->expecting_intr = false;        // 等待硬盘中断

        lock_init(&channel->lock);  

        /* 将信号量初始化为0 */
        sema_init(&channel->disk_done, 0);
        register_handler(channel->irq_no, intr_hd_handler);
        
        /* 分别获取两个硬盘的参数及分区信息 */
        while(dev_no < 2){
            struct disk* hd = &channel->devices[dev_no];
            hd->my_channel = channel;
            hd->dev_no = dev_no;
            sprintf(hd->name, "sd%c", 'a' + channel_no * 2 + dev_no);
            identify_disk(hd);      // 获取硬盘参数
            if(dev_no != 0){            // 内核本身的硬盘不处理               
                partition_scan(hd, 0);              // 扫描硬盘上的分区
                printk("\n after scan slave partition\n");
            }
            p_no = 0, l_no = 0;
            dev_no++;
        }
        dev_no = 0;     // 驱动号置位0，为下一个channel的硬盘初始化
        channel_no++;
    }
    printk("\n   all partition info\n");
    /* 打印所有分区信息 */
    list_traversal(&partition_list, partition_info, (int)NULL);
    printk("ide_init done\n");
}


