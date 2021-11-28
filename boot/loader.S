%include "boot.inc"

; loader段
section loader vstart=LOADER_BASE_ADDR          ; 加载到0x900      

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
    
    GDT_SIZE  equ $ - GDT_BASE
    GDT_LIMIT  equ  GDT_SIZE-1               ; GDT 的界限，长度减一

    times 60 dq 0       ; 保留60个描述符的空位
;    段描述符的索引 
    SELECTOR_CODE equ (0x0001 << 3) + TI_GDT + RPL0
    SELECTOR_DATA equ (0x0002 << 3) + TI_GDT + RPL0
    SELECTOR_VIDEO equ (0x0003 << 3) + TI_GDT + RPL0
; loader.bin写入到0x900,前面总共有 64 * 8 = 512 = 0x200大小的内容,这里就是0xb00
    total_mem_bytes dd 0        ; 保存内存容量，字节为单位

    ; 定义GDTR的初始值
    gdt_ptr     dw  GDT_LIMIT
            dd GDT_BASE

; 地址范围描述符
    ards_buf times 244 db 0  
    ards_num dw 0               ; ards的数量
    
;     mem_volume_err db 'get mem bytes error.'
;     loadermsg  db ' 2 loader in real.'  
;-------------------------------
loader_start:
;------------ 进行内存容量检测 --------------------
;------------ 调用0xe820获取内存 ------------------
        xor ebx, ebx            ; 第一次调用置0,之后会自动修改他     
        mov di,  ards_buf       ; ES:DI 保存ARDS的内容
        mov edx, 0x534d4150            ; 签名
        
.call_0xE820:
        xor eax, eax
        mov eax, 0x0000e820
        mov ecx, 20             ; ARDS结构的字节大小
        int 0x15        ; 调用0x15功能
        ; 判断是否调用成功
        jc .call_0xE801           

        ; 调用没有出错
        add di, cx              ; 每次后移20字节大小
        inc word [ards_num]          ; 增加一个ards数量

        ; ebx的值为0,则这是最后一次调用
        cmp ebx, 0
        jne .call_0xE820

        ; 确定内存的容量: 找到ards描述的最大内存值即最大
        ; 冒泡排序找到最大值
        mov cx, [ards_num]      
        mov ebx, ards_buf       
        xor edx, edx            ; 保存最大的内存容量
.find_max_mem_area:
        mov eax, [ebx]
        add eax, [ebx + 8]      ; LengthLow: 内存的长度低32位
        add ebx, 20             ; 下一个ards
        cmp edx, eax    

        jg .continue
        mov edx, eax    ; 小，则edx获取到较大值
.continue:
        loop .find_max_mem_area ; 下一次循环

        ; 获得结果的话
        jmp .get_mem_byte

;------------- 调用 0xe801 --------------------
.call_0xE801:
        mov ax, 0xe801
        int 0x15      

        jc .call_0x88  ; 是否调用成功
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
        
.call_0x88:
        mov ah, 0x88
        int 0x15

        jc .error_hlt
        and eax, 0x0000FFFF

        ; 计算内存大小
        xor edx,edx
        mov bx, 1024    ; AX * 1024 + 1MB       
        mul bx          ; 可能超出AX的限制    
        shl edx,16
        or edx, eax
        add edx, 0x100000     ; 加上1MB， 因为只返回1MB以上的内存

        ; jmp .get_mem_byte

; .error_hlt:
;         mov sp, LOADER_BASE_ADDR      
;         mov bp, mem_volume_err       ; ES:BP = 字符串地址
;         mov cx, 17              ; CX 字符串长度
;         mov ax, 0x1301          ; AH 子功能号  AL 显示输出方式
;         mov bx, 0x001f          ; 页号为0, 蓝底粉红字
;         mov dx, 0x1800          
;         int 0x10                ; 10号中断
;         jmp $

; 填写mem_byte的内容
.get_mem_byte:
        mov [total_mem_bytes], edx
        
; ; 显示字符串
;         mov sp, LOADER_BASE_ADDR      
;         mov bp, loadermsg       ; ES:BP = 字符串地址
;         mov cx, 17              ; CX 字符串长度
;         mov ax, 0x1301          ; AH 子功能号  AL 显示输出方式
;         mov bx, 0x001f          ; 页号为0, 蓝底粉红字
;         mov dx, 0x1800          
;         int 0x10                ; 10号中断

      ;  cli             ; 关闭中断

      
        ; 打开A20地址线
        in al,0x92
        or al, 0000_0010B
        out 0x92,al

        lgdt [gdt_ptr]             ; 加载GDT

        ; 打开PE位
        mov eax, cr0
        or eax, 0x00000001
        mov cr0, eax

; 刷新流水线，进入保护模式
        jmp dword SELECTOR_CODE: ProtectMode
.error_hlt:
        hlt

        [bits 32]
ProtectMode:

        mov ax, SELECTOR_DATA              ; 数据段选择子
        mov ds, ax
        mov es, ax
        mov ss, ax
        mov esp, LOADER_STACK_TOP      
        ; 显示 Protect mode OK
        mov ax, SELECTOR_VIDEO             ; Video段索引,TI = 0, RPL = 0
        mov gs, ax
        
      ;  mov byte [gs: 0x60],'P' 
      ;  mov byte [gs: 0x61], 0x81

      ;  mov byte [gs: 0x62],'S' 
      ;  mov byte [gs: 0x63], 0x81

; -------------- 加载kernel ------------------------
        mov eax, KERNEL_START_SECTOR            ; 内核对应的起始扇区号
        mov ebx, KERNEL_BIN_BASE_ADDR            ; 加载到对应的地址
        mov ecx, 200                     ; 读入的扇区数
        call rd_disk_32

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
    ;   mov byte [gs:160], 'O'

; 刷新流水线
        jmp SELECTOR_CODE:enter_kernel
; ----------- 进入内核 ---------------------------------
enter_kernel:
        call kernel_init                ; <!-- TODO:调用之后无法返回,是elf文件格式的问题吗？
        mov esp, 0xc009f000             ; 为了整齐，设置为了9f000

     ;  mov byte [gs:170], 'K'  

        jmp KERNEL_ENTRY_POINT          ; 跳转到内核入口处

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
        ; e_pentsize offset=42 程序头表每个条目的大小 2B
        ; e_phnum offset=44 记录段的个数,2B

        ; 段本身： p_type offset=0 段的类型 4B大小
        mov dx, [KERNEL_BIN_BASE_ADDR + 42]     ; 程序头的大小
        mov ebx, [KERNEL_BIN_BASE_ADDR + 28]    ; 段的开始位置
        add ebx, KERNEL_BIN_BASE_ADDR           ; 地址
        mov cx, [KERNEL_BIN_BASE_ADDR + 44]     ; 段的个数，也是循环次数
.each_segment: 
        ; 判断 p_type是否为 PT_NULL, p_type 4B大小, 
        cmp dword [ebx + 0], PT_NULL
        je .PTNULL

       ; mov eax, [ebx+8]
       ; cmp eax, 0xc0001500     ; 判断虚拟地址是否为开始地址
       ; jb .PTNULL

        ; 参数入栈，调用mem_cpy进行段复制
        ; dst src size 从右向左开始
        push dword [ebx + 16]           ; 段在文件当中的大小 p_filesz

        mov eax, [ebx + 4]      ; 段在文件中的位置 p_offset
        add eax , KERNEL_BIN_BASE_ADDR          ; 加上文件的基地址
        push eax        ; src
        
        push dword [ebx + 8]           ; dst, 本段在内存当中的起始虚拟地址
        call mem_cpy             ; 段复制
        add esp, 12             ; 清理栈当中的三个参数
.PTNULL:
        add ebx, edx
        loop .each_segment              
        ret

; 拷贝内核执行文件的段到指定位置
; 参数： dst src size， 从右向左开始入栈
mem_cpy:
        cld     ; 清楚 DF,让dst和src的参数自动增加
        push ebp
        mov ebp, esp    ; 栈从上向下扩展，注意这里栈的内容，调用前入栈了函数的返回地址

        push ecx        ; 压栈保存

        mov edi, [ebp + 8]
        mov esi, [ebp + 12]
        mov ecx, [ebp + 16]
        rep movsb

        pop ecx
        pop ebp
        ret

; ----------------- 准备页目录表和页表 ---------------------
setup_page:
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
        or eax, PG_RW_W | PG_US_U | PG_P        ; 可写 1 ，普通用户 0，处于内存中 1
        mov [PAGE_DIR_TABLE_POS], eax           ;  0 号页目录表
        mov [PAGE_DIR_TABLE_POS + 0xc00], eax   ; 0xc0000000以上 对应的页目录表项是第 768个
        
        ; 最后一个页表项指向页目录表自己的地址
        sub eax, 0x1000
        mov [PAGE_DIR_TABLE_POS + 4092], eax
        ; 一个页表能够覆盖4MB的空间，总共包含1024个页表项
        mov ecx, 256            ; 只用到1MB的内存，所以只需要256个页表项即可
        mov esi, 0
        mov edx, PG_US_S | PG_RW_W | PG_P       ; 属性为7, US=1超级用户, W=1, P=1
    ; 内循环创建页表项,只初始化访问属性，指向的地址为0
    .create_pte:
        mov [ebx + esi * 4], edx

        add edx, 4096
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
        loop .create_kernel_pde
        
        ret


;-------------------------------------------------------------------------------
			   ;功能:读取硬盘n个扇区
rd_disk_32:	   
;-------------------------------------------------------------------------------
							 ; eax=LBA扇区号
							 ; ebx=将数据写入的内存地址
							 ; ecx=读入的扇区数
      mov esi,eax	   ; 备份eax
      mov di,cx		   ; 备份扇区数到di
;读写硬盘:
;第1步：设置要读取的扇区数
      mov dx,0x1f2
      mov al,cl
      out dx,al            ;读取的扇区数

      mov eax,esi	   ;恢复ax

;第2步：将LBA地址存入0x1f3 ~ 0x1f6

      ;LBA地址7~0位写入端口0x1f3
      mov dx,0x1f3                       
      out dx,al                          

      ;LBA地址15~8位写入端口0x1f4
      mov cl,8
      shr eax,cl
      mov dx,0x1f4
      out dx,al

      ;LBA地址23~16位写入端口0x1f5
      shr eax,cl
      mov dx,0x1f5
      out dx,al

      shr eax,cl
      and al,0x0f	   ;lba第24~27位
      or al,0xe0	   ; 设置7～4位为1110,表示lba模式
      mov dx,0x1f6
      out dx,al

;第3步：向0x1f7端口写入读命令，0x20 
      mov dx,0x1f7
      mov al,0x20                        
      out dx,al

;;;;;;; 至此,硬盘控制器便从指定的lba地址(eax)处,读出连续的cx个扇区,下面检查硬盘状态,不忙就能把这cx个扇区的数据读出来

;第4步：检测硬盘状态
  .not_ready:		   ;测试0x1f7端口(status寄存器)的的BSY位
      ;同一端口,写时表示写入命令字,读时表示读入硬盘状态
      nop
      in al,dx
      and al,0x88	   ;第4位为1表示硬盘控制器已准备好数据传输,第7位为1表示硬盘忙
      cmp al,0x08
      jnz .not_ready	   ;若未准备好,继续等。

;第5步：从0x1f0端口读数据
      mov ax, di	   ;以下从硬盘端口读数据用insw指令更快捷,不过尽可能多的演示命令使用,
			   ;在此先用这种方法,在后面内容会用到insw和outsw等

      mov dx, 256	   ;di为要读取的扇区数,一个扇区有512字节,每次读入一个字,共需di*512/2次,所以di*256
      mul dx
      mov cx, ax	   
      mov dx, 0x1f0
  .go_on_read:
      in ax,dx		
      mov [ebx], ax
      add ebx, 2

      loop .go_on_read
      ret

; ; --------
; rd_disk_32:                     ; eax 扇区号
;                                 ; ebx 读取到的位置
;                                 ; ecx 扇区数量
;         mov esi, eax    ; 暂时保存
;         mov di, cx
;         ; 在Primary通道读取
;         ; 设置sector count
;         mov dx, 0x1f2
;         mov al, cl
;         out dx, al

;         mov eax, esi
;         ; 设置LBA的28位地址,从LBAlow开始
;         inc dx          ; 0x1f3
;         out dx,al

;         ;设置LBAmid
;         inc dx          ; 0x1f4
;         shr eax,8 
;         out dx,al

;         ;设置LBAhigh
;         shr eax,8
;         inc dx
;         out dx,al

;         ; 设置device
;         inc dx
;         shr eax,8
;         and al,00001111B
;         or al,11100000B     
;         out dx,al

;         ; 输入命令给command, 开始读取
;         mov dx,0x1f7        
;         mov al,0x20
;         out dx,al       ;写入command

;     .busy:
;         ; nop
;         in al, dx           ; 读取Status
;         and al,10001000B      ; 判断最高位，硬盘是否正忙
;         cmp al,00001000B
;         jnz .busy           ; 是，则循环等待，直到硬盘准备完成

;         ; 开始正式读取
;         mov ax, di      ; 将cx的值移动到ax当中
;         mov dx,256
;         mul dx          
;         mov cx,ax       ;计算总共需要读取多少个字

;         mov dx,0x1f0
;     .readword:
;         in ax,dx        ; 读取对应数据
;         mov [ebx], ax
;         add ebx,2
;         loop .readword
      
;         ret
    





