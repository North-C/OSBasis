[bits 32]
%define ERROR_CODE nop  

; 压入占位的0,替代ERROR_CODE的位置空间
%define ZERO push 0 

extern idt_table
section .data


global intr_entry_table
intr_entry_table:

; 定义 VECTOR宏，方便后续进行声明
%macro VECTOR 2         
section .text
intr%1entry:
    %2          ; 占位置

    push ds
    push es
    push fs
    push gs
    pushad

    ; push intr_str      
    ; call put_str       ; 打印中断发生信息
    ; add esp, 4         ; 跳过参数

    mov al, 0x20    ; 操作OCW2, EOI设为1,中断结束通知
    out 0xa0, al
    out 0x20, al

    push %1     ; 压入中断号    对应vec_nr参数

    call [idt_table + %1 * 4]   ; 中断处理函数 调用general_intr
    jmp intr_exit

section .data       ; 编译器会将属性相同的section合并成一个segment, 之后所有的中断都会结合到一起，形成连续的数组
    dd intr%1entry  ; 存储中断处理程序的入口地址
%endmacro

section .text
global intr_exit
intr_exit:
    add esp, 4      ; 越过中断号
    popad
    pop gs
    pop fs
    pop es
    pop ds
    add esp, 4      ; 跳过error_code
    iretd            ; 从中断返回

VECTOR 0x00, ZERO
VECTOR 0x01, ZERO
VECTOR 0x02, ZERO
VECTOR 0x03, ZERO 
VECTOR 0x04, ZERO
VECTOR 0x05, ZERO
VECTOR 0x06, ZERO
VECTOR 0x07, ZERO           ; 不包含错误码
VECTOR 0x08, ERROR_CODE     ; 包含错误码的中断
VECTOR 0x09, ZERO
VECTOR 0x0a, ERROR_CODE
VECTOR 0x0b, ERROR_CODE 
VECTOR 0x0c, ZERO
VECTOR 0x0d, ERROR_CODE
VECTOR 0x0e, ERROR_CODE
VECTOR 0x0f, ZERO 
VECTOR 0x10, ZERO
VECTOR 0x11, ERROR_CODE
VECTOR 0x12, ZERO
VECTOR 0x13, ZERO 
VECTOR 0x14, ZERO
VECTOR 0x15, ZERO
VECTOR 0x16, ZERO
VECTOR 0x17, ZERO 
VECTOR 0x18, ERROR_CODE
VECTOR 0x19, ZERO
VECTOR 0x1a, ERROR_CODE
VECTOR 0x1b, ERROR_CODE 
VECTOR 0x1c, ZERO
VECTOR 0x1d, ERROR_CODE
VECTOR 0x1e, ERROR_CODE
VECTOR 0x1f, ZERO 
VECTOR 0x20, ZERO
