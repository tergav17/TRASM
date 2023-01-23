#!/bin/bash
../as_r src/putc.s ; mv a.out obj/putc.o
../as_r src/puts.s ; mv a.out obj/puts.o
../as_r src/getc.s ; mv a.out obj/getc.o
../as_r src/hello.s ; mv a.out obj/hello.o
rm lib/liba.a
ar r lib/liba.a obj/getc.o obj/putc.o obj/puts.o
