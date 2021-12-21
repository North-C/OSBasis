#ifndef __KERNEL_INTERRUPT_H
#define __KERNEL_INTERRUPT_H
#include "stdint.h"
typedef void* intr_handler;
void idt_init(void);
enum intr_status{
    INT_OFF,        // 默认值为0
    INT_ON          // 默认值为1
};
enum intr_status enable_intr(void);
enum intr_status disable_intr(void);
enum intr_status get_intr_status(void);
enum intr_status set_intr_status(enum intr_status);
void register_handler(uint8_t vec_nr, intr_handler function);
#endif