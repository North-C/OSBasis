#!/bin/bash
# 用root执行
dd if=./build/mbr.bin of=/usr/bin/hd60M.img bs=512 count=1 conv=notrunc
dd if=./build/loader.bin of=/usr/bin/hd60M.img bs=512 seek=2 count=3 conv=notrunc  
dd if=./build/kernel.bin of=/usr/bin/hd60M.img bs=512 seek=9 count=200 conv=notrunc
