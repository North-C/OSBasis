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
    %2          ; 错误码,或者占位置

    push ds     ; 保护所有寄存器映像
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

    push %1     ; 压入中断号 对应vec_nr参数

    call [idt_table + %1 * 4]   ; 中断处理函数 调用general_intr
    jmp intr_exit   

section .data       ; 编译器会将属性相同的section合并成一个segment, 之后所有的中断都会结合到一起，形成连续的数组
    dd intr%1entry  ; 存储中断处理程序的入口地址
%endmacro

section .text
global intr_exit
intr_exit:          ; 退出中断，恢复上下文环境
    add esp, 4      ; 越过中断号
    popad           ; 进入中断时的逆过程 （不会管esp）
    pop gs
    pop fs
    pop es
    pop ds
    add esp, 4      ; 跳过error_code
    iretd            ; 从中断返回，进入iret

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
; 注册8259A的全部中断，16个IR引脚
VECTOR 0x20, ZERO   ; 时钟中断
VECTOR 0x21, ZERO   ; 键盘中断
VECTOR 0x22, ZERO   ; 级联用的
VECTOR 0x23, ZERO   ; 串口2对应的入口
VECTOR 0x24, ZERO   ; 串口1对应的入口
VECTOR 0x25, ZERO   ; 并口2对应的入口
VECTOR 0x26, ZERO   ; 软盘对应
VECTOR 0x27, ZERO   ; 并口1对应
VECTOR 0x28, ZERO   ; 实时时钟
VECTOR 0x29, ZERO   ; 重定向
VECTOR 0x2a, ZERO   ; 保留
VECTOR 0x2b, ZERO   ; 保留
VECTOR 0x2c, ZERO   ; ps/2鼠标
VECTOR 0x2d, ZERO   ; fpu 浮点单元异常
VECTOR 0x2e, ZERO   ; 硬盘
VECTOR 0x2f, ZERO   ; 保留

; 0x80号中断
[bits 32]
extern syscall_table
section .text
global syscall_handler
syscall_handler:
    push 0          ; 占据错误码的位置，不能落下

    push ds     ; 保护所有寄存器映像
    push es
    push fs
    push gs
    pushad
    
    ; 参数系统调用号
    push  0x80            
    ; 传入参数
    push edx            ; 三个参数
    push ecx
    push ebx

    ; 调用处理函数
    call [syscall_table + eax * 4]   ; 中断处理函数 调用syscall_table
    add esp, 12         ; 移动栈顶位置，越过3个参数

    mov [esp + 8 * 4], eax      ; 将eax的值存入内核栈当中eax的位置
    jmp intr_exit  
