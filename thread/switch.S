[bits 32]
section .text
global switch_to
switch_to:

    push esi
    push edi
    push edx
    push ebp

    ; 备份当前线程的环境
    mov eax, [esp + 20]      ; 找到当前线程cur_thread
    mov [eax], esp          ; cur_thread最上面的就是self_kstack,偏移为0
                            ; 保存栈顶指针
    ; 进入下一个线程的环境
    mov eax, [esp + 24]
    mov esp, [eax]      ; 换上下一个pcb的kernel_kstack成员

    pop ebp
    pop edx
    pop edi
    pop esi
    ret     