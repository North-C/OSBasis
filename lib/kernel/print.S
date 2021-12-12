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

section .data
put_int_buffer  dq      0      ; 定义8字节缓冲区用于数字到字符的转换

[bits 32]
section .text
; ------------------- put_str -----------------------
; 输入： 栈中的参数     无输出，显示字符串即可
global put_str
put_str:        ; 调用put_char
        push ebx
        push ecx
        xor ecx, ecx
        mov ebx, [esp + 12]     ; 需要打印的参数地址

.goon:
        mov cl, [ebx]
        cmp cl, 0       ; 字符串的结尾为ASCII=0,即\0字符
        jz .str_over
        push ecx        ; 将要打印的字符传入栈中
        call put_char
        add esp, 4      ; 回收栈空间: 传递的ecx参数
        inc ebx         ; 下一个字符
        jmp .goon
.str_over:
        pop ecx
        pop ebx
        ret
; -------------------- put_char -------------------
; 80 *25的文本显示模式下，一个屏幕可以显示2000个字（字符），显存总共32KB
; 两种方案：1. 将初始地址始终设置为0，一直增长直到32KB再清除旧字符。
;         2. 将一个屏幕大小设置为基本单位，从0到2000，满了需要进行清除。
; 将栈当中的1个字符写入到光标所在处
global put_char
put_char:
        pushad          ; 备份寄存器环境
        mov ax, SELECTOR_VIDEO
        mov gs, ax      ; 将gs设置为视频段寄存器

;--  获取当前光标位置 ---------
        mov dx, 0x03d4  ; 索引寄存器
        mov al, 0x0e    ; 光标的高8位
        out dx, al
        mov dx, 0x03d5  ; 数据读写端口，获得或设置光标位置
        in al, dx
        mov ah, al
        
        ; 获取低8位
        mov dx, 0x03d4
        mov al, 0x0f
        out dx, al
        mov dx, 0x03d5
        in al, dx

        ; 将光标存入bx
        mov bx, ax
        mov ecx, [esp + 36]     ; pushad 所有通用寄存器和ESP,ESI,EDI,EBP，4Byte * 8 = 32字节
                                ; 加上主函数的返回地址 四个字节，所以从第36个字节开始
        cmp cl, 0x0d    ; 回车符CR
        jz .is_carriage_return
        cmp cl, 0x0a     ; 换行符
        jz .is_line_feed

        cmp cl, 0x08     ; 回退backspace
        jz .is_backspace
        jmp .put_other

.is_backspace:
; -------- backspace ---------
; 处理流程：回退，即将光标向前面一个显存位置移动即可；
; 但同时也需要将原来位置的字符修改为空白，以免显得怪异
        dec bx
        shl bx, 1

        mov byte [gs:bx], 0x20  ; 修改的字符
        inc bx
        mov byte [gs:bx], 0x07  ; 字符属性
        shr bx, 1       ; 光标一次占两个字节
        jmp .set_cursor
;-------------------------

.put_other:
        shl bx, 1       ; 光标一次用两个字节

        mov [gs:bx], cl
        inc bx
        mov byte [gs:bx], 0x07
        shr bx, 1       ; 原来的光标值
        inc bx
        cmp bx, 2000    ; 光标值小于2000则没有到最后
        jl .set_cursor

; 处理换行和回车
.is_line_feed:
.is_carriage_return:    ; 移动到行首即可
        xor dx, dx
        mov ax, bx
        mov si, 80
        div si
        sub bx, dx      ; 去掉余数取整，便回到行首

.is_carriage_return_end:
        add bx, 80      ; 判断是否需要滚屏
        cmp bx, 2000    ;; 2000个字
.is_line_feed_end:
        jl .set_cursor

.roll_screen:           ; 滚屏，将屏幕的1~24行搬运到0~23行
        cld
        mov ecx, 960    ; 2000-80= 1920字符，一次搬运四个，一共搬运960次
        mov esi, 0xb80a0     ; 第一行行首
        mov edi, 0xb8000      ; 第0行行首
        rep movsd

; 最后一行设置为空白
        mov ebx, 3840
        mov ecx, 80

.cls:
        mov word [gs:ebx], 0x0720       ; 黑底白字的空格
        add ebx, 2
        loop .cls
        mov bx, 1920    ; 重置光标，放到最后一行的行首

.set_cursor:                    ; 重置光标位置
        ; 设置高8位
        mov dx, 0x03d4
        mov al, 0x0e
        out dx, al
        mov dx, 0x03d5
        mov al, bh
        out dx, al

        ; 设置低8位
        mov dx, 0x03d4
        mov al, 0x0f
        out dx, al
        mov dx, 0x03d5
        mov al,bl
        out dx, al
.put_char_done:
        popad
        ret

; ------------ 打印整数 -----------------------
; 将数字转换成对于的字符，只支持32位整数
; 输入：栈当中的数字   输出：屏幕上打印数字，不打印前缀
global put_int
put_int:
        pushad
        mov ebp, esp    ; 利用ebp进行栈操作
        mov eax, [ebp + 4 * 9]  ; call的返回地址+4 * 8= 4* 9
        mov edx, eax
        mov edi, 7      ; print_int_buffer当中的偏移量
        mov ecx, 8      
        mov ebx, put_int_buffer

; 按照16进制从低到高位打印
.16based_4bits:
        and edx, 0x0000000F     ; 只有低四位有效
        cmp edx, 9
        jg .is_A2F
        add edx, '0'
        jmp .store
.is_A2F:
        sub edx, 10     
        add edx, 'A'

.store:
        mov [ebx+edi], dl       ; 低位数字保存到高地址，大端存储
        dec edi
        shr eax, 4              ; 下一个16进制数字
        mov edx, eax
        loop .16based_4bits
; print_int_buffer当中的字符全部打印完成
.ready_to_print:
        inc edi                 ; 下一个位置
.skip_prefix_0:
        cmp edi, 8              ; 已经全部比较完成
        je .full0
; 找出连续的0字符
.go_on_skip:
        mov cl, [put_int_buffer + edi]
        inc edi
        cmp cl, '0'
        je .skip_prefix_0       ; 判断下一位
        dec edi                  
        jmp .put_each_num       

.full0:
        mov cl, '0'                     ; 内容全部为0， 则只打印0
.put_each_num:
        push ecx                
        call put_char
        add esp, 4
        inc edi
        mov cl, [put_int_buffer + edi]
        cmp edi, 8
        jl .put_each_num
        popad
        ret

;-------------
global set_cursor
set_cursor:
        pushad
        mov bx, [esp+36]
        ; 先设置光标的高8位
        mov dx, 0x03d4  ; 索引寄存器
        mov al, 0x0e    ; 高8位
        out dx, al
        mov dx, 0x03d5  ; 进行读写
        mov al, bh
        out dx, al

        ; 设置低8位
        mov dx, 0x03d4
        mov al, 0x0f
        out dx, al
        mov dx, 0x03d5
        mov al, bl
        out dx, al
        popad
        ret
