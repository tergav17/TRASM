#!/bin/bash
../ld_r -v obj/hello.o lib/liba.a
mv a.out out/a.out
z80dasm -l -g 0x0000 -o out/a.asm out/a.out
cp out/a.out out/arel.out
../reloc_r -sn out/arel.out 0x1000
z80dasm -l -g 0x1000 -o out/arel.asm out/arel.out
