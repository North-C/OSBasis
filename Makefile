BUILD_DIR= ./build
ENTRY_POINT= 0xc0001500
AS = nasm
CC = gcc
LD = ld
LIB = -I lib/ -I lib/kernel/ -I lib/user/ -I kernel/ -I device/ -I thread/ -I userprog/
ASFLAGS = -f elf
CFLAGS = -Wall -m32 -c -fno-stack-protector $(LIB) -c -fno-builtin -W -Wstrict-prototypes -Wmissing-prototypes
LDFLAGS = -m elf_i386 -Ttext $(ENTRY_POINT) -e main -Map $(BUILD_DIR)/kernel.map
OBJS = $(BUILD_DIR)/main.o $(BUILD_DIR)/init.o $(BUILD_DIR)/interrupt.o \
      $(BUILD_DIR)/timer.o $(BUILD_DIR)/kernel.o $(BUILD_DIR)/print.o \
      $(BUILD_DIR)/debug.o $(BUILD_DIR)/memory.o $(BUILD_DIR)/bitmap.o \
      $(BUILD_DIR)/string.o  $(BUILD_DIR)/thread.o $(BUILD_DIR)/list.o \
	  $(BUILD_DIR)/switch.o  $(BUILD_DIR)/sync.o $(BUILD_DIR)/console.o \
	  $(BUILD_DIR)/keyboard.o $(BUILD_DIR)/ioqueue.o $(BUILD_DIR)/tss.o \
	  $(BUILD_DIR)/process.o $(BUILD_DIR)/syscall_init.o  $(BUILD_DIR)/syscall.o \
	  $(BUILD_DIR)/stdio.o $(BUILD_DIR)/ide.o $(BUILD_DIR)/stdio-kernel.o
# $< 依赖文件当中的第一个文件
# $@ 规则中的目标文件名集合，所有目标文件
############################### C代码编译 ######################
$(BUILD_DIR)/main.o: kernel/main.c lib/kernel/print.h \
      lib/stdint.h kernel/init.h  device/ioqueue.h userprog/process.h \
	  lib/user/syscall.h kernel/memory.h lib/stdio.h
	$(CC) $(CFLAGS) $< -o $@          

$(BUILD_DIR)/init.o: kernel/init.c kernel/init.h lib/kernel/print.h \
      lib/stdint.h kernel/interrupt.h device/timer.h device/keyboard.h \
	  userprog/syscall_init.h 
	$(CC) $(CFLAGS) $< -o $@

$(BUILD_DIR)/interrupt.o: kernel/interrupt.c kernel/interrupt.h \
      lib/stdint.h kernel/global.h lib/kernel/io.h lib/kernel/print.h
	$(CC) $(CFLAGS) $< -o $@

$(BUILD_DIR)/timer.o: device/timer.c device/timer.h lib/stdint.h \
      lib/kernel/io.h   lib/kernel/print.h
	$(CC) $(CFLAGS) $< -o $@

$(BUILD_DIR)/debug.o: kernel/debug.c kernel/debug.h \
      lib/kernel/print.h lib/stdint.h kernel/interrupt.h
	$(CC) $(CFLAGS) $< -o $@

$(BUILD_DIR)/string.o: lib/string.c lib/string.h lib/stdint.h kernel/global.h \
	lib/stdint.h kernel/debug.h
	$(CC) $(CFLAGS) $< -o $@

$(BUILD_DIR)/bitmap.o: lib/kernel/bitmap.c lib/kernel/bitmap.h \
      lib/stdint.h 	lib/kernel/print.h  lib/string.h lib/stdint.h \
     	lib/kernel/print.h kernel/interrupt.h kernel/debug.h
	$(CC) $(CFLAGS) $< -o $@

$(BUILD_DIR)/memory.o: kernel/memory.c kernel/memory.h lib/stdint.h lib/kernel/bitmap.h \
   	kernel/global.h  kernel/debug.h lib/kernel/print.h \
	lib/kernel/io.h kernel/interrupt.h lib/string.h lib/stdint.h 
	$(CC) $(CFLAGS) $< -o $@

$(BUILD_DIR)/thread.o: thread/thread.c thread/thread.h lib/stdint.h \
        kernel/global.h lib/kernel/bitmap.h kernel/memory.h lib/string.h \
        lib/stdint.h lib/kernel/print.h kernel/interrupt.h kernel/debug.h
	$(CC) $(CFLAGS) $< -o $@

$(BUILD_DIR)/list.o: lib/kernel/list.c lib/kernel/list.h kernel/interrupt.h  lib/stdint.h kernel/global.h \
		lib/kernel/io.h lib/kernel/print.h thread/thread.h kernel/memory.h
	$(CC) $(CFLAGS) $< -o $@

$(BUILD_DIR)/sync.o: thread/sync.c thread/sync.h kernel/interrupt.h  lib/stdint.h kernel/global.h \
		lib/kernel/io.h lib/kernel/print.h thread/thread.h kernel/debug.h kernel/memory.h 
	$(CC) $(CFLAGS) $< -o $@

$(BUILD_DIR)/console.o: device/console.c device/console.h kernel/interrupt.h  lib/stdint.h kernel/global.h \
		lib/kernel/io.h lib/kernel/print.h thread/sync.h
	$(CC) $(CFLAGS) $< -o $@

$(BUILD_DIR)/keyboard.o: device/keyboard.c device/keyboard.h kernel/interrupt.h lib/kernel/io.h lib/kernel/print.h \
		kernel/global.h kernel/debug.h thread/thread.h thread/sync.h kernel/global.h kernel/memory.h
	$(CC) $(CFLAGS) $< -o $@

$(BUILD_DIR)/ioqueue.o: device/ioqueue.c device/ioqueue.h kernel/interrupt.h lib/kernel/io.h lib/kernel/print.h \
		kernel/global.h kernel/debug.h thread/thread.h thread/sync.h kernel/global.h lib/stdint.h kernel/memory.h
	$(CC) $(CFLAGS) $< -o $@

$(BUILD_DIR)/tss.o: userprog/tss.c userprog/tss.h kernel/interrupt.h lib/kernel/io.h lib/kernel/print.h \
		kernel/global.h lib/stdint.h lib/string.h kernel/memory.h thread/thread.h
	$(CC) $(CFLAGS) $< -o $@

$(BUILD_DIR)/process.o: userprog/process.c userprog/process.h kernel/interrupt.h lib/kernel/io.h lib/kernel/print.h \
		kernel/global.h lib/stdint.h lib/string.h kernel/memory.h thread/thread.h device/console.h  \
		userprog/tss.h lib/kernel/list.h lib/kernel/bitmap.h lib/string.h kernel/debug.h
	$(CC) $(CFLAGS) $< -o $@

$(BUILD_DIR)/syscall_init.o: userprog/syscall_init.c userprog/syscall_init.h kernel/interrupt.h lib/kernel/io.h lib/kernel/print.h \
		kernel/global.h lib/stdint.h lib/string.h kernel/memory.h thread/thread.h device/console.h 	thread/sync.h \
		lib/user/syscall.h lib/kernel/print.h
	$(CC) $(CFLAGS) $< -o $@

$(BUILD_DIR)/syscall.o: lib/user/syscall.c lib/user/syscall.h kernel/interrupt.h lib/kernel/io.h lib/kernel/print.h \
		kernel/global.h lib/stdint.h lib/string.h kernel/memory.h thread/thread.h device/console.h 	
	$(CC) $(CFLAGS) $< -o $@

$(BUILD_DIR)/stdio.o: lib/stdio.c lib/stdio.h kernel/interrupt.h lib/kernel/io.h lib/kernel/print.h \
		kernel/global.h lib/stdint.h lib/string.h thread/thread.h \
		lib/user/syscall.h
	$(CC) $(CFLAGS) $< -o $@

$(BUILD_DIR)/ide.o: device/ide.c device/ide.h lib/stdint.h thread/sync.h \
    	lib/kernel/list.h kernel/global.h thread/thread.h lib/kernel/bitmap.h \
     	kernel/memory.h lib/kernel/io.h lib/stdio.h lib/stdint.h lib/kernel/stdio-kernel.h \
	kernel/interrupt.h kernel/debug.h device/console.h device/timer.h lib/string.h
	$(CC) $(CFLAGS) $< -o $@

$(BUILD_DIR)/stdio-kernel.o: lib/kernel/stdio-kernel.c lib/kernel/stdio-kernel.h lib/stdint.h \
    	lib/kernel/print.h lib/stdio.h lib/stdint.h device/console.h kernel/global.h
	$(CC) $(CFLAGS) $< -o $@
######################### 汇编代码编译 #############################
$(BUILD_DIR)/kernel.o: kernel/kernel.S
	$(AS) $(ASFLAGS) $< -o $@

$(BUILD_DIR)/print.o: lib/kernel/print.S
	$(AS) $(ASFLAGS) $< -o $@
	
$(BUILD_DIR)/switch.o: thread/switch.S
	$(AS) $(ASFLAGS) $< -o $@
#######################################################################################
$(BUILD_DIR)/kernel.bin: $(OBJS)
	$(LD) $(LDFLAGS) $^ -o $@

.PHONY : mk_dir hd clean all

mk_dir:
	if [[ ! -d $(BUILD_DIR) ]]; then mkdir $(BUILD_DIR); fi

hd:
	dd  if=$(BUILD_DIR)/kernel.bin   \
        of=/usr/bin/hd60M.img bs=512 count=200 seek=9 conv=notrunc

clean:
	cd $(BUILD_DIR) && rm -f ./*

build: $(BUILD_DIR)/kernel.bin

all: mk_dir build hd
