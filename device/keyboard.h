#ifndef __DEVICE_KEYBOARD_H
#define __DEVICE_KEYBOARD_H
#include "ioqueue.h"
void keyboard_init(void);
extern struct ioqueue kbd_buf;
#endif
