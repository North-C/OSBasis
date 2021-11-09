%include "boot.inc"
SECTION MBR vstart=0x7c00

    mov ax,cs
    mov ds,ax
    mov es,ax
    mov ss,ax
    mov gs,ax
    mov sp,0x7c00
    mov ax,0xb800
    mov fs,ax

; Clear screen
; function: int 0x00
   mov ax,0002h
   int 10H
; Only bx can be used as Index register
    ; xor bx,bx

    ; mov byte [fs: bx+0x10], 'G'
    ; mov byte [fs: bx+0x11], 0x81

    mov byte [fs: 0x00], 'I'
    mov byte [fs: 0x01], 10000001B

    mov byte [fs: 0x02], 'n'
    mov byte [fs: 0x03], 10000001B

    mov byte [fs: 0x04], ' '
    mov byte [fs: 0x05], 0x81

    mov byte [fs: 0x06], 'M'
    mov byte [fs: 0x07], 0x81

    mov byte [fs: 0x08], 'B'
    mov byte [fs: 0x09], 0x81

    mov byte [fs: 0x0a], 'R'
    mov byte [fs: 0x0b], 0x81

    ; mov byte [fs: 0x0c], 'i'
    ; mov byte [fs: 0x0d], 0x81

    ; mov byte [fs: 0x0e], 't'
    ; mov byte [fs: 0x0f], 0x81

; 调用硬盘读取函数
    mov cx,4                ; 设置需要加载的loader的大小
    mov si,LOADER_BASE_ADDR
    mov eax,LOADER_START_SECTOR
    call ata_lba_read

    jmp LOADER_BASE_ADDR
    ; jmp $

;----------------------------
; Read disk
;-----------------------------
; read  n sectors 
        ; input: eax LBA扇区号
        ;        si 读取后存储的位置
        ;        cx 要读取的扇区数

ata_lba_read:
        push ax
        push bx
        push dx
        push cx

        push eax

        ; 在Primary通道读取
        ; 设置sector count
        mov dx,0x1f2
        mov ax,cx
        out dx,al

        pop eax
        ; 设置LBA的28位地址,从LBAlow开始
        inc dx
        out dx,al

        ;设置LBAmid
        inc dx
        shr eax,8 
        out dx,al

        ;设置LBAhigh
        shr eax,8
        inc dx
        out dx,al

        ; 设置device
        inc dx
        shr eax,8
        and al,00001111B
        or al,11100000B     
        out dx,al

        ; 输入命令给command, 开始读取
        mov dx,0x1f7        
        mov al,0x20
        out dx,al       ;写入command

    .busy:
        in al, dx           ; 读取Status
        and al,10001000B      ; 判断最高位，硬盘是否正忙
        cmp al,00001000B
        jnz .busy           ; 是，则循环等待，直到硬盘准备完成

        ; 开始正式读取
        pop ax      ; 弹出cx的值到ax当中
        mov bx,256
        mul bx          
        mov cx,ax       ;计算总共需要读取多少个字

        mov dx,0x1f0
    .readword:
        in ax,dx        ; 读取对应数据
        mov [si], ax
        add si,2
        loop .readword

        ; pop ax
        ; mov bx,256
        ; xor dx,dx
        ; mul bx
        ; mov cx,ax   
        ; mov dx,0x1f0
        ; mov di,si
        ; rep insw        

        ; 64位的参考
        ; mov rax, 256         ; to read 256 words = 1 sector
        ; xor dx, dx
        ; mov bl, cl           ; read CL sectors
        ; mul bx
        ; mov rcx, rax         ; RCX is counter for INSW
        ; mov rdx, 0x1F0       ; Data port, in and out
        ; rep insw             ; in to [RDI]
        pop dx
        pop bx
        pop ax
        ret
    
    times 510-($-$$) db 0
            db 0x55, 0xaa
>>>>>>> refs/remotes/origin/master
