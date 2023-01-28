#!/bin/bash
../ld_r -v obj/hello.o lib/liba.a
z80dasm -l -g 0x0000 -o a.asm a.out
