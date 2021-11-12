%include "boot.inc"

; loader段
section loader vstart=LOADER_BASE_ADDR          ; 加载到0x900

    LOADER_STACK_TOP equ LOADER_BASE_ADDR       
    jmp loader_start

; 确定GDT的位置
    GDT_BASE  dd 0x00000000        ; GDT的第一个段描述符，规定要设置为0
              dd 0x00000000

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

; -------------- 加载kernel ------------------------
        mov eax, KERNEL_START_SECTOR            ; 内核对应的起始扇区号
        mov si, KERNEL_BIN_BASE_ADDR            ; 加载到对应的地址
        mov cx, 200                     ; 读入的扇区数
        call ata_lba_read

; -------------- 创建页目录表和页表 -------------------
        ; 创建页目录及页表并初始化页内存位图
        call setup_page

        ; 将GDT的基地址和偏移量写入 gdt_ptr对应的地址
        sgdt [gdt_ptr]

        ; 获取段基地址
        mov ebx, [gdt_ptr + 2]
        ; GDT中的video段的段地址加上 0xc0000000
        or dword [gdt_ptr + 0x18 + 4], 0xc0000000       
        ; GDT的基地址也映射到内核虚拟空间
        add dword [gdt_ptr + 2], 0xc0000000

        add esp, 0xc0000000
        ; 将页目录表的地址赋给cr3---PDTR
        mov eax, PAGE_DIR_TABLE_POS
        mov cr3, eax

        ; 打开cr0的PG位(最高位)
        mov eax, cr0
        or eax, 0x80000000
        mov cr0, eax

        ; 重新加载gdt
        lgdt [gdt_ptr]

        ; 通过视频段显示 来测试是否更新成功
        mov byte [gs:160], 'V'

; 刷新流水线
        jmp SELECTOR_CODE:enter_kernel
; ----------- 进入内核 ---------------------------------
enter_kernel:
        call kernel_init
        mov esp, 0xc009f000                     ;??
        jmp KERNEL_ENTRY_POINT          ; 跳转到内核入口处

; ----------------- 准备页目录表和页表 ---------------------
set_page:
        ; 将页目录占用的空间逐字节清空
        mov ecx, 4096
        mov esi, 0
    .clear_page_byte:
        mov byte [PAGE_DIR_TABLE_POS + esi], 0
        inc esi
        loop .clear_page_byte

    ; 循环创建页目录表项        
    .create_pde:
        mov eax, PAGE_DIR_TABLE_POS     ; 页目录表
        add eax, 0x10000        ; 第一个页表的位置
        mov ebx, eax    ; ebx保存页表基地址
        
        ; 处理目录项0和目录项768，将其都指向第一个页表，用于映射内核的地址
        or eax, PG_RW_W | PG_US_U | PG_P        ; 可写，普通用户，处于内存中
        mov [PAGE_DIR_TABLE_POS], eax
        mov [PAGE_DIR_TABLE_POS + 0xc00], eax  ; 0xc0000000以上 对应的页目录表项是第 768个
        
        ; 最后一个页表项指向页目录表自己的地址
        sub eax, 0x1000
        mov [PAGE_DIR_TABLE_POS + 4092], eax
        ; 一个页表能够覆盖4MB的空间，总共包含1024个页表项
        mov ecx, 256            ; 只用到1MB的内存，所以只需要256个页表项即可
        mov esi, 0
        or edx, PG_US_S | PG_RW_W | PG_P
    ; 内循环创建页表项,只初始化访问属性，指向的地址为0
    .create_pte:
        mov [ebx + esi * 4], edx

        add ebx, 0x1000
        inc esi
        loop .create_pte
; 创建其他PTE的页表项

        mov eax, PAGE_DIR_TABLE_POS
        add eax, 0x2000
        or eax, PG_RW_R | PG_US_U | PG_P
        mov ebx, PAGE_DIR_TABLE_POS

        mov ecx, 254      ; 额外创建
        mov esi, 769
    .create_kernel_pde:
        mov [ebx + esi * 4], eax
        add eax, 0x1000
        inc esi
        loop .create_kernel_pte
        
        ret
; ---------------- 拷贝kernel.bin中的segment到编译好的虚拟地址,即内存的程序映象 ----------------
; 分析程序中的segment
kernel_init:
        xor eax, eax
        xor ebx, ebx    ; 记录程序头表地址
        xor ecx, ecx    ; 记录程序头表中的program header数量
        xor edx, edx    ; 记录program header的尺寸

        ; 通过ELF的格式来进行加载
        ; e_entry offset=24 入口地址, 4B
        ; e_phoff offset=28 程序头表的偏移量位置 4B
        ; e_phnum offset=44 记录段的个数,2B

        ; 段本身： p_type offset=0 段的类型 4B大小

; 拷贝内核执行文件的段到指定位置
mem_cpy:
                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                    







