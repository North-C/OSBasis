#ifndef __KERNEL_GLOBAL_H
#define __KERNEL_GLOBAL_H
#include "stdint.h"


#define RPL0 0
#define RPL1 1
#define RPL2 2
#define RPL3 3

#define TI_GDT 0
#define TI_LDT 1

#define G_BYTE  0
#define G_4K    1 
#define DB_16    0
#define DB_32    1   
#define L_64CODE    1   
#define L_OTHER     0
#define AVL     0
#define GDT_P   1
#define GDT_DESC_DPL0   0   
#define GDT_DESC_DPL1   1
#define GDT_DESC_DPL2   2
#define GDT_DESC_DPL3   3   
#define S_SYS       0   
#define S_CODE      1       // 非系统段-代码段
#define S_DATA      S_CODE    // 非系统段-数据段
#define TYPE_CODE   8       // 1000 
#define TYPE_DATA   2       // W = 1,可读写 E=0,向上扩展  
#define TYPE_TSS    9       // 1001 B=0

// 内核GDT描述符
#define SELECTOR_K_CODE ((1<<3) + (TI_GDT<<2)+ RPL0)
#define SELECTOR_K_DATA ((2<<3) + (TI_GDT<<2)+ RPL0)
#define SELECTOR_K_STACK SELECTOR_K_DATA
#define SELECTOR_K_GS ((3<<3) + (TI_GDT<<2)+ RPL0)
// 用户进程GDT描述符
#define SELECTOR_U_CODE ((5<<3) + (TI_GDT<<2)+ RPL3)
#define SELECTOR_U_DATA ((6<<3) + (TI_GDT<<2)+ RPL3)
#define SELECTOR_U_STACK SELECTOR_U_DATA

#define GDT_ATTR_HIGH ((G_4K << 7) + (DB_32 <<6) + (L_OTHER << 5 ) +(AVL << 4))
#define GDT_ATTR_LOW_CODE_DPL3    ((GDT_P << 7) + (GDT_DESC_DPL3 << 5) + (S_CODE << 4) + TYPE_CODE )
#define GDT_ATTR_LOW_DATA_DPL3    ((GDT_P << 7) + (GDT_DESC_DPL3 << 5) + (S_DATA << 4) + TYPE_DATA )

//   IDT描述符属性
#define IDT_DESC_P      1
#define IDT_DESC_DPL0   0
#define IDT_DESC_DPL3   3
#define IDT_DESC_32_TYPE    0xE     // 32位的门

    // IDT的高32位中设置P DPL 和TYPE
#define IDT_DESC_ATTR_DPL0  ( (IDT_DESC_P << 7) + (IDT_DESC_DPL0 << 5) + IDT_DESC_32_TYPE )
#define IDT_DESC_ATTR_DPL3  ( (IDT_DESC_P << 7) + (IDT_DESC_DPL3 << 5) + IDT_DESC_32_TYPE )

// 数据类型设计
#define NULL ((void*)0)
#define bool int
#define true 1
#define false 0

// TSS描述符属性
#define TSS_DESC_D 0
#define TSS_ATTR_HIGH ((G_4K << 7) + (TSS_DESC_D << 6) +(L_OTHER << 5) +(AVL << 4) + 0x0)
#define TSS_ATTR_LOW  ((GDT_P << 7) + (GDT_DESC_DPL0 << 5) +(S_SYS << 4) + TYPE_TSS)
// TSS的描述符选择子
#define SELECTOR_TSS ((4 << 3) + (TI_GDT << 2) + RPL0)

/* GDT的描述符结构 */
struct gdt_desc{
    uint16_t low_limit_word;     // 低32位的段界限
    uint16_t low_base_word;   // 低32位的段基地址
    uint8_t mid_base_byte;      // 中间的基地址
    uint8_t low_attr_byte;      // 高32位的低端属性
    uint8_t limit_high_attr_byte;     
    uint8_t high_base_byte;     // 高32位的段基地址
};

#endif
