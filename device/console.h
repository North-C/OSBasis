#ifndef __DEVICE_CONSOLE_H
#define __DEVICE_CONSOLE_H

/* 实现终端的互斥有序输出 */

void console_init(void);
void console_acquire(void);
void console_release(void);
void console_put_str(char* str);
void console_put_char(uint8_t s);
void console_put_int(uint32_t num);
#endif
