#!/bin/bash

as -o init.o init.s --32
gcc -c -o main.o main.c -nostdlib -nostartfiles -nodefaultlibs -nostdinc -ffreestanding -m32
ld -o pci.o -T lscpt.lds init.o main.o -melf_i386 ../lib/p5.o ../lib/p5s.o
objcopy -O binary -j .init -j .text -j .data -j .bss --set-section-flags .bss=alloc,load,contents pci.o pci.mod
cp pci.mod ../../../rampak/
