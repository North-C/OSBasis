.PHONY:build image clean

img=/home/john/os/hd30M.img

mbr_src=mbr.S
loader_src=loader.S

mbr=mbr.bin
loader=loader.bin

mbr_loader:
    nasm -I build/include/ -o boot/mbr.bin boot/mbr.S
    nasm -I build/include/ -o boot/loader.bin boot/loader.S

build:
    #nasm -f elf -o lib/kernel/print.o lib/kernel/print.S
    #gcc -m32 -I lib/kernel -c -o kernel/main.o  kernel/main.c
    #ld -m elf_i386 -Ttext 0xc0001500 -e main -o kernel/kernel.bin kernel/main.o lib/kernel/print.o

    gcc -m32 -I lib/kernel -I lib/ -I kernel/ -c -fno-builtin -o build/main.o kernel/main.c
    nasm -f elf -o build/print.o lib/kernel/print.S
    nasm -f elf -o build/kernel.o kernel/kernel.S
    gcc -m32 -I lib/kernel/ -I lib/ -I kernel/ -c -fno-builtin -o build/interrupt.o kernel/interrupt.c
    gcc -m32 -I lib/kernel -I lib/ -I kernel -c -fno-builtin -o build/init.o kernel/init.c
    ld -m elf_i386 -Ttext 0xc0001500 -e main -o kernel/kernel.bin kernel/main.o lib/kernel/print.o

image:
  #  @-rm -rf $(img)
  #  bximage -hd -mode="flat" -size=30 -q $(img)
    
    dd if=./build/loader.bin of=/usr/bin/hd60M.img bs=512 seek=2 count=3 conv=notrunc  
    dd if=./build/kernel.bin of=/usr/bin/hd60M.img bs=512 seek=9 count=200 conv=notruncdd if=./build/mbr.bin of=/usr/bin/hd60M.img bs=512 count=1 conv=notrunc
