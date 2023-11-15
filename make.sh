#!/bin/bash

nasm -f as86 -o loader.o loader.asm || exit 1
bcc -0 -Md -o kernel.o kernel.c -c -A -0 || exit 2
ld86 -s -d -o rom_code.bin loader.o kernel.o || exit 1

truncate -s 8192 rom_code.bin || exit 1
#truncate -s 65536 rom_code.bin || exit 1

objdump -D -Mintel,i8086 -b binary -m i8086 rom_code.bin

./patch2pnprom rom_code.bin || exit 1

nasm -o bootsect.bin bootsect.asm || exit 1

gcc -o serserv serserv.c || exit 1
