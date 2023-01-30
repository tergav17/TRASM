#!/bin/bash
../ld_r -v obj/hello.o lib/liba.a
z80dasm -l -g 0x0000 -o a.asm a.out
cp a.out arel.out
../reloc_r -v arel.out 0x1000
z80dasm -l -g 0x1000 -o arel.asm arel.out
