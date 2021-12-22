#include "global.h"
#include "tss.h"
#include "memory.h"
#include "string.h"
#include "thread.h"
#include "print.h"  
#define PG_SIZE 4096

static struct gdt_desc make_gdt_desc(uint32_t* desc_addr, uint32_t limit, \
                                                uint8_t attr_high, uint8_t attr_low);


/* 任务状态段tss结构 */
struct tss{
    uint32_t backlink;
    uint32_t* esp0;     // 应该是指针,指向栈顶
    uint32_t ss0;
    uint32_t* esp1;
    uint32_t ss1;
    uint32_t* esp2;
    uint32_t ss2;
    uint32_t cr3;
    uint32_t (*eip)(void*);           // eip指向需要执行的指令
    uint32_t eflags;
    uint32_t eax;
    uint32_t ecx;
    uint32_t edx;
    uint32_t ebx;
    uint32_t esp;
    uint32_t ebp;
    uint32_t esi;
    uint32_t edi;
    uint32_t es;
    uint32_t cs;
    uint32_t ss;
    uint32_t ds;
    uint32_t fs;
    uint32_t gs;
    uint32_t ldt;
    uint32_t trace;         // 
    uint32_t io_base;       // IO位图的偏移地址
};

static struct tss tss;

/* 更新tss中esp0字段的值为pthread的0级栈 */
void update_tss_esp(struct task_struct* pthread){
    // 修改esp0
    tss.esp0 = (uint32_t*)((uint32_t) pthread + PG_SIZE);   
}

/* 创建gdt描述符, desc_addr指向GDT描述符 */
static struct gdt_desc make_gdt_desc(uint32_t* desc_addr, uint32_t limit, uint8_t attr_high, uint8_t attr_low){
    uint32_t desc_base = (uint32_t)desc_addr;
    struct gdt_desc desc ;
    desc.low_limit_word = (limit & 0x0000ffff);
    desc.low_base_word = (desc_base & 0x0000ffff);
    desc.mid_base_byte = ((desc_base &0x00ff0000)>>16);
    desc.low_attr_byte = (uint8_t)attr_low;
    desc.limit_high_attr_byte = (((limit & 0x000f0000) >> 16) + (uint8_t)attr_high);
    desc.high_base_byte = ((desc_base & 0xff000000)>>24);
    return desc;
}

/* 加载TSS描述符到GDT中,并且重新加载GDT */
void tss_init(){
    /* 初始化TSS */
    put_str("tss_init start\n");
    // TSS所有字节初始化为0 
    uint32_t tss_size = sizeof(tss);
    memset( &tss, 0, tss_size);
    tss.ss0 = SELECTOR_K_STACK;     // 内核栈
    tss.io_base = tss_size;
    
    // TSS描述符
    // TSS作为第四个描述符,放到第4个位置, 0x900 + 0x20 = 0x920
    *((struct gdt_desc*)0xc0000920) = make_gdt_desc((uint32_t*)0, tss_size - 1, TSS_ATTR_HIGH, TSS_ATTR_LOW);
    
    // 用户代码段描述符
    *((struct gdt_desc*)0xc0000928) = make_gdt_desc((uint32_t*)0, (uint32_t) 0xfffff, GDT_ATTR_HIGH, GDT_ATTR_LOW_CODE_DPL3);
    
    // 用户数据段描述符
    *((struct gdt_desc*)0xc0000930) = make_gdt_desc((uint32_t*)0, (uint32_t) 0xfffff, GDT_ATTR_HIGH, GDT_ATTR_LOW_DATA_DPL3);
    
    // gdt的32位基地址 和 16位limit
    uint64_t gdt_operand = ( ((uint64_t)(uint32_t)0xc0000900 << 16) | (8 * 7-1));

    // 汇编
    // lgdt重新加载GDT
    asm volatile("lgdt %0": :"m"(gdt_operand));     // `lgdt`: 注意这里的先后顺序,如果反过来会出错
    // 设置TR指向TSS
    asm volatile("ltr %w0": :"r" (SELECTOR_TSS));   // 向TR中加载16位的TSS选择子, 约束: `m`和 `r`都可以, `w`设置为16位大小
    put_str("tss_init done\n");
}

