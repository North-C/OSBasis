;-------- loader ---------------
LOADER_BASE_ADDR equ 0x900
LOADER_START_SECTOR equ 0x2


PAGE_DIR_TABLE_POS equ 0x100000         ; 页目录表的基地址，超出1MB的首地址

;---------- GDT -----------------
; 选择子属性
RPL0 equ 00b
RPL1 equ 01b
RPL2 equ 10b
RPL3 equ 11b
TI_GDT equ 000b
TI_LDT equ 100b

;---------- Page Table ----------
PG_P equ 1b
PG_RW_R equ 00b     ; 表示PTE和PGE的RW属性值为R,即可读
PG_RW_W equ 10b     ; W 表示可写
PG_US_S equ 000b    ; 表示PTE和PGE的US属性值为S,即0 Supervisor
PG_US_U equ 100b    ; 表示PTE和PGE的US属性值为U,即1 User


; -------- 加载内核 ---------------
PT_NULL equ 0

KERNEL_START_SECTOR equ 9
KERNEL_BIN_BASE_ADDR equ 0x700000
KERNEL_ENTRY_POINT equ 0xc0001500
