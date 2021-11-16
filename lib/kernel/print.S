; 处理流程：
; 保留寄存器现场
; 获取光标坐标，即下一个可打印字符的位置
; 获取待打印的字符
; 判断字符是否为控制字符; 回车，换行，退格进行相应处理
; 判断是否需要滚屏
; 更新光标坐标
; 恢复现场

TI_GDT equ 0
RPL0 equ 0
SELECTOR_VIDEO equ (0x0003 << 3) + TI_GDT + RPL0

[bits 32]
section .text
; -------------------- put_char -------------------
; 
global put_char
put_char:
        pushad
        mov ax, SELECTOR_VIDEO
        mov gs, ax
        












