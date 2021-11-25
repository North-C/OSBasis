.PHONY:build image clean

img=/home/john/os/hd30M.img

mbr_src=mbr.S
loader_src=loader.S

mbr=mbr.bin
loader=loader.bin

mbr_loader:
    nasm -I boot/include/ -o boot/mbr.bin boot/mbr.S
    nasm -I boot/include/ -o boot/loader.bin boot/loader.S

build:
    nasm -f elf -o lib/kernel/print.o lib/kernel/print.S
    gcc -m32 -I lib/kernel -c -o kernel/main.o  kernel/main.c
    ld -m elf_i386 -Ttext 0xc0001500 -e main -o kernel/kernel.bin kernel/main.o lib/kernel/print.o
# <!-- TODO: 书上说链接的时候，尽量保持调用在前，实现在后。看一下<Linkers and Loaders >求证一下具体过程。
image:
  #  @-rm -rf $(img)
  #  bximage -hd -mode="flat" -size=30 -q $(img)
    dd if=./boot/mbr.bin of=/usr/bin/hd60M.img bs=512 count=1 conv=notrunc
    dd if=./boot/loader.bin of=/usr/bin/hd60M.img bs=512 seek=2 count=3 conv=notrunc  
    dd if=./kernel/kernel.bin of=/usr/bin/hd60M.img bs=512 seek=9 count=200 conv=notrunc

clean:
    @-rm -rf boot/*.img boot/*.bin boot/*.o /boot/*~
    @-rm -rf lib/*.img lib/*.bin lib/*.o lib/*~
    @-rm -rf lib/kernel/*.img lib/kernel/*.bin lib/kernel/*.o lib/kernel/*~
    @-rm -rf kernel/*.img kernel/*.bin kernel/*.o kernel/*~
    @-rm -rf *.o *.bin *.img *~