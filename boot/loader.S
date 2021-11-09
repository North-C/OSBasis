%include "boot.inc"

; loader段
section loader vstart=LOADER_BASE_ADDR          ; 加载到0x900

    LOADER_STACK_TOP equ LOADER_BASE_ADDR       
    jmp loader_start

; 确定GDT的位置
    GDT_BASE  dd 0x0000 0000        ; GDT的第一个段描述符，规定要设置为0
              dd 0x0000 0000

; 构建段描述符  
    CODE_SEG dd 0x0000ffff         ; 低32位 段基址 段界限
             dd 0x00CF9800          ; 4k,32位,非64位，界限1111, P=1,0级,非系统段，
                            ; 可执行，非依从，不可读，a清0: 1000
    ; 数据段 和 栈段 定义到一起
    DATA_STACK_SEG  dd 0x0000ffff
                dd 0x00CF9200   

    ; 音频段，显示文本
    VIDEO_SEG  dd 0x80000007     ; 从0xb8000 到 0xbffff 的文本显示区，4KB为粒度进行分段，分为7段
               dd 0x00C0920b     ; 本质是一个数据段

    GDT_LIMIT     dw   ($-GDT_BASE)-1               ; GDT 的界限，长度减一

    times 60 dq 0       ; 保留60个描述符
    
    ; 定义GDTR的初始值
    GDTR     dw  GDT_LIMIT
            dd GDT_BASE

    loadermsg  db ' 2 loader in real.'  

; 地址范围描述符
    ards_buf times 244 db 0     
    ards_num dw 0               ; ards的数量

    mem_byte  dd 0              ; 保存检测到的内存的大小

;-------------------------------
; Enter Protect mode
loader_start:
;------------ 调用0xe820获取内存 ------------------
        xor eax, eax
        mov eax, 0xE820

        xor ebx, ebx            ; 第一次调用置0,之后会自动修改他
        mov es,  LOADER_BASE_ADDR     ; ES:DI 保存ARDS的内容
        mov di,  ards_buf       
        mov ecx, 20             ; ARDS结构的字节大小
        mov edx, 0x534d4150            ; 签名
        
.call_0xE820:
        int 0x15        ; 调用0x15功能
        ; 判断是否调用成功
        jc .call_0xE820             

        ; 调用没有出错
        add di, cx              ; 每次后移20字节大小
        inc [ards_num]          ; 增加一个ards数量

        ; ebx的值为0,则这是最后一次调用
        cmp ebx, 0
        jne .call_0xE820

        ; 确定内存的容量: 找到ards描述的最大内存值即最大

        ; 获得结果的话
        jmp .get_mem_byte
;------------- 调用 0xe801 --------------------
        mov ax, 0xE801
.call_0xE801:
        int 0x15      

        jc .call_0xE801  ; 是否调用成功
        ; AX 保存15MB以下内存数量  BX 保存16~4GB内存数量， 计算大小
        xor edx, edx
        mov dx, ax
        shl edx, 10
        mov esi, edx            ; 暂时保存

        xor edx,edx
        mov dx,bx
        shl edx, 16
        add edx, esi

        jmp .get_mem_byte
;------------- 调用0x88 ------------------------
        mov ax, 0x88
.call_0x88:
        int 0x15

        jc .call_0x88
        ; 计算内存大小
        xor edx,edx
        mov bx, 1024    ; AX * 1024 + 1MB       
        mul bx          ; 可能超出AX的限制
        add dx, 0x10    
        shl edx,16
        or edx, ax

; 填写mem_byte的内容
.get_mem_byte:
        mov [mem_byte], edx


; 显示字符串
        mov sp, LOADER_BASE_ADDR      
        mov bp, loadermsg       ; ES:BP = 字符串地址
        mov cx, 17              ; CX 字符串长度
        mov ax, 0x1301          ; AH 子功能号  AL 显示输出方式
        mov bx, 0x001f          ; 页号为0, 蓝底粉红字
        mov dx, 0x1800          
        int 0x10                ; 10号中断

        cli             ; 关闭中断

        lgdt [GDTR]             ; 加载GDT

        ; 打开A20地址线
        in al,0x92
        or al, 0000_0010B
        out 0x92,al

        ; 打开PE位
        mov eax, cr0
        or eax, 0x00000001
        mov cr0, eax

; 刷新流水线，进入保护模式
        jmp dword 0x0008: ProtectMode

    [bits 32]
ProtectMode:
               
        mov ax, 0x0010              ; 数据段选择子
        mov ds, ax
        mov es, ax
        mov ss, ax
        mov esp, LOADER_STACK_TOP      

        ; 显示 Protect mode OK
        mov ax, 0x0018             ; Video段索引,TI = 0, RPL = 0
        mov gs, ax
        
        mov byte [gs: 0x60],'P' 
        mov byte [gs: 0x61], 0x81

        mov byte [gs: 0x62],'S' 
        mov byte [gs: 0x63], 0x81


        jmp $

       
        














