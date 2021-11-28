#include "interrupt.h"
#include "stdint.h"
#include "global.h"
#include "io.h"
#include "print.h"

#define IDT_DESC_CNT 0X21   // 一共支持的中断总数

#define PIC_M_CTRL 0x20     // 主片的控制端口
#define PIC_M_DATA 0x21     // 主片的数据端口

#define PIC_S_CTRL 0xa0     // 从片的控制端口
#define PIC_S_DATA 0xa1     // 从片的数据端口

// 中断描述符的结构体
struct gate_desc{
    uint16_t func_offset_low_word;
    uint16_t selector;
    uint8_t dcount;     // 双字计数字段

    uint8_t attribute;
    uint16_t func_offset_high_word;         
};

// idt是中断描述符表，数组形式
static struct gate_desc idt[IDT_DESC_CNT];     

// 声明中断处理函数入口数组
// intr_handler是一个空指针类型，没有具体类型，仅用来表示地址
extern intr_handler intr_entry_table[IDT_DESC_CNT];     

char* intr_name[IDT_DESC_CNT];      //用于保存异常的名字
intr_handler idt_table[IDT_DESC_CNT];   // 目标中断处理函数数组


/* 初始化可编程中断控制器8259A */
static void pic_init(void){
    // 初始化主片 
    outb(PIC_M_CTRL, 0x11);      //ICW1 边沿触发，级联8259,需要ICW4
    outb(PIC_M_DATA, 0x20);      // ICW2, 设置起始中断向量号
    outb(PIC_M_DATA, 0x04);      // ICW3,设置IRQ2连接从片
    outb(PIC_M_DATA, 0x01);      // ICW4, 8086模式，正常EOI

    // 初始化从片
    outb(PIC_S_CTRL, 0x11);
    outb(PIC_S_DATA, 0x28);
    outb(PIC_S_DATA, 0x02);
    outb(PIC_S_DATA, 0x01);

    // 打开主片上的IR0,即只接受时钟中断
    outb(PIC_M_DATA, 0xfe);
    outb(PIC_S_DATA, 0xff);

    put_str("  pic_init done\n");
}


/* 创建中断门描述符 
    intr_handler 表示对应中断处理程序的入口地址
*/
static void make_idt_desc(struct gate_desc* p_desc, uint8_t attr, intr_handler function){
    p_desc->func_offset_low_word = (uint32_t)function & 0x0000FFFF;
    p_desc->selector = SELECTOR_K_CODE;
    p_desc->dcount = 0;
    p_desc->attribute = attr;
    p_desc->func_offset_high_word = ((uint32_t)function & 0xFFFF0000) >> 16;
}

/* 初始化IDT */
static void idt_desc_init(void){
    int i;
    for(i = 0; i < IDT_DESC_CNT; i++){
        make_idt_desc(&idt[i], IDT_DESC_ATTR_DPL0, intr_entry_table[i]);
    }
    put_str(" idt_desc_init done\n");
}

/* 完成所有中断的初始化和加载工作 */
void idt_init(){
    put_str("start to init IDT\n");
    idt_desc_init();
    pic_init();

    /* 加载IDT */
    // 前面是 sizeof(idt)-1表示界限， 后面则是idt的地址
    uint64_t idt_operand =( (sizeof(idt) - 1) | ((uint64_t)(uint32_t)idt << 16)) ;
    asm volatile("lidt %0": : "m" (idt_operand));   // 输入 m约束, 输入变量为idt_operand
    put_str("idt_init done\n");
}

/* 通用的中断处理函数，一般用在异常出现时的处理 */
static void general_intr_handler(uint8_t vec_nr){

}
/* 一般中断处理函数注册及异常名称注册 */
static void exception_init(void){
    
}

