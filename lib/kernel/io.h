// 和端口有关的操作函数
#ifndef __LIB_IO_H
#define __LIB_IO_H
#include "stdint.h"

/* 向端口写入一个字节 */
static inline void outb(uint16_t port, uint8_t data){
    // "a"对应%b0,即al  "N“表示从0～255的立即数 d--dx
    asm volatile("outb %b0, %w1": :"a"(data), "Nd"(port));
}

/* 将addr处起始的word_cnt个字写入端口port */
static inline void outsw(uint16_t port, const void* addr, uint32_t word_cnt){
    // + 既做输入也做输出
    // S 表示esi/si  c 表示ecx/cx/cl  d---edx/dx/dl
    asm volatile("cld; rep outsw": "+S"(addr), "+c"(word_cnt): "d"(port));
}

/* 从端口port获取一个字节 */
static inline uint8_t inb(uint16_t port){
    uint8_t data;
    asm volatile("inb %w1, %b0" : "=a"(data) : "Nd" (port));
    return data;
}

/* 将从端口port读入的word_cnt个字写入addr */ 
static inline void insw(uint16_t port, void* addr, uint32_t word_cnt){
    // dx输入port, EDI和ECX表示目的地址和字数，既做输入也做输出 "memory"主动声明修改了内存，同时清楚寄存器缓存
    asm volatile ("cld; rep insw" : "+D"(addr), "+c"(word_cnt) : "d" (port) : "memory");
}
    
#endif
