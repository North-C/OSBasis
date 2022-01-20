#include "interrupt.h"
#include "stdint.h"
#include "global.h"
#include "io.h"
#include "print.h"

#define IDT_DESC_CNT 0x30   // 总共支持的中断总数

#define PIC_M_CTRL 0x20     // 主片的控制端口
#define PIC_M_DATA 0x21     // 主片的数据端口

#define PIC_S_CTRL 0xa0     // 从片的控制端口
#define PIC_S_DATA 0xa1     // 从片的数据端口

#define EFLAGS_IF 0x00000200        // 设置IF
#define GET_EFLAGS(EFLAGS_VAR) asm volatile("pushfl; popl %0;": "=g" (EFLAGS_VAR))    // 通过栈输出到EFLAGS_VAR

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

/* 注册中断处理函数 vec_nr即中断向量号，function是中断处理函数 */
void register_handler(uint8_t vec_nr, intr_handler function){
    idt_table[vec_nr] = function;
}

/* 初始化可编程中断控制器8259A */
static void pic_init(void){
    // 初始化主片 
    outb(PIC_M_CTRL, 0x11);      //ICW1 边沿触发，级联8259,需要ICW4
    outb(PIC_M_DATA, 0x20);      // ICW2, 设置起始中断向量号，为0x20
    outb(PIC_M_DATA, 0x04);      // ICW3,设置IRQ2连接从片
    outb(PIC_M_DATA, 0x01);      // ICW4, 8086模式，正常EOI

    // 初始化从片
    outb(PIC_S_CTRL, 0x11);
    outb(PIC_S_DATA, 0x28);
    outb(PIC_S_DATA, 0x02);
    outb(PIC_S_DATA, 0x01);

    // 打开主片上的IR0,即只接受时钟中断 0xfe
    outb(PIC_M_DATA, 0xfe);     // 测试键盘，只打开键盘中断，其他全部关闭 0xfd ; 打开时钟和键盘中断 0xfc
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

/* 通用的中断处理函数，一般用在异常出现时的处理 */
// vec_nr表示中断向量号
static void general_intr_handler(uint8_t vec_nr){
    // IRQ7和IRQ15产生伪中断(Spurious interrupt)，无需处理。
    if(vec_nr == 0x27 || vec_nr == 0x2f){       // ICW2设置的起始向量号0x20 + 接口号IRQ7和IRQ15
        return ;
    }
    /* 优化一下输出 */
    set_cursor(0);  // 将光标重置到屏幕最左上角
    put_str("!!!!! execution message  start !!!!!\n");
    set_cursor(88);
    put_str("!!!!! ");
    put_str(intr_name[vec_nr]);
    if(vec_nr == 14){       // 如果是缺页中断Page fault
        int page_fault_vaddr = 0;
        asm volatile("movl %%cr2, %0": "=r"(page_fault_vaddr));     // r eax/ebx/ecx/edx/edi/esi中的任意一个
        put_str("\npage fault addr is "); put_int(page_fault_vaddr);
    }
    put_str("\n!!!!  execution message end !!!!\n");
    while(1);
    /* // 实际的处理也只是输出一下中断向量号
    put_str("int vector: 0x");  
    put_int(vec_nr);
    put_char('\n');
 */    
}

/* 一般中断处理函数注册及异常名称注册 */
static void exception_init(void){
    int i;
    for(i=0; i< IDT_DESC_CNT; i++){
        // 注册函数
        idt_table[i] = general_intr_handler;
        intr_name[i] = "Unknown";
    }
    // 注册异常名称
    intr_name[0] = "#DE Devide Error";
    intr_name[1] = "#DB Debug Exception";
    intr_name[2] = "NMI Interrupt";
    intr_name[3] = "#BP Breakpoint Exception";
    intr_name[4] = "#OF Overflow Exception";
    intr_name[5] = "#BR BOUND Range Exceeded Exception";
    intr_name[6] = "#UD Invalid Opcode Exception";
    intr_name[7] = "#NM Device Not Available Exception";
    intr_name[8] = "#DF Double Fault Exception";
    intr_name[9] = "Coprocessor Segment Overrun";
    intr_name[10] = "#TS Invalid TSS Exception";
    intr_name[11] = "#NP Segment Not Present";
    intr_name[12] = "#SS Stack Fault Exception";
    intr_name[13] = "#GP General Protection Exception";
    intr_name[14] = "#PF Page-Fault Exception";
    // intr_name[15] 第15项是intel保留项，未使用
    intr_name[16] = "#MF x87 FPU Floating-Point Error";
    intr_name[17] = "#AC Alignment Check Exception";
    intr_name[18] = "#MC Machine-Check Exception";
    intr_name[19] = "#XF SIMD Floating-Point Exception";
}

/* 打开中断，返回开中断之前的状态 */
enum intr_status enable_intr(void){
    enum intr_status old_status;
    if(INT_ON == get_intr_status()){
        old_status = INT_ON;
        return old_status;
    }else{
        old_status = INT_OFF;
        asm volatile("sti");
        return old_status;
    }
}

/* 关闭中断，返回关闭以前的状态 */
enum intr_status disable_intr(void){
    enum intr_status old_status;
    if(INT_ON == get_intr_status()){
        old_status = INT_ON;
        asm volatile("cli": : :"memory");
        return old_status;
    }else{
        old_status = INT_OFF;
        return old_status;
    }
}

/* 获取当前的中断状态 */
enum intr_status get_intr_status(void){
    uint32_t eflags;
    GET_EFLAGS(eflags);
     return eflags & EFLAGS_IF ? INT_ON : INT_OFF;
}

/* 依据status来打开或者关闭中断，返回原来的中断状态 */
enum intr_status set_intr_status(enum intr_status status){
    return status & INT_ON ? enable_intr() : disable_intr();
}

/* 完成所有中断的初始化和加载工作 */
void idt_init(){
    put_str("start to init IDT\n");
    idt_desc_init();        // 初始化IDT
    exception_init();       // 注册中断处理函数和异常名称
    pic_init();             // 初始化8259A

    /* 加载IDT */
    // 前面是 sizeof(idt)-1表示界限， 后面则是idt的地址
    uint64_t idt_operand =( (sizeof(idt) - 1) | ((uint64_t)(uint32_t)idt << 16)) ;
    asm volatile("lidt %0": : "m" (idt_operand));   // 输入 m约束, 输入变量为idt_operand
    put_str("idt_init done\n");
}