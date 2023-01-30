; z80dasm 1.1.6
; command line: z80dasm -l -g 0x1000 -o arel.asm arel.out

	org	01000h

	jr l1010h
	inc bc	
	nop	
	djnz $-59
	nop	
	nop	
	nop	
	nop	
	daa	
	nop	
	scf	
	nop	
	ld d,a	
	nop	
l1010h:
	ld hl,01029h
	call sub_1017h
	ret	
sub_1017h:
	ld a,(hl)	
	or a	
	ret z	
	call sub_1021h
	inc hl	
	jp sub_1017h
sub_1021h:
	ld b,001h
	call 01005h
	ret	
	add hl,hl	
	djnz l1072h
	ld h,l	
	ld l,h	
	ld l,h	
	ld l,a	
	inc l	
	jr nz,$+121
	ld l,a	
	ld (hl),d	
	ld l,h	
	ld h,h	
	ld a,(bc)	
	nop	
	rlca	
	nop	
	ld (bc),a	
	ld de,00100h
	inc d	
	nop	
	ld bc,0001bh
	ld bc,0001fh
	ld bc,00024h
	ld (bc),a	
	daa	
	nop	
	nop	
	nop	
	nop	
	inc b	
	nop	
	ld l,b	
	ld h,l	
	ld l,h	
	ld l,h	
	ld l,a	
	ld e,a	
	ld (hl),e	
	nop	
	ld (bc),a	
	add hl,hl	
	djnz $+114
	ld (hl),l	
	ld (hl),h	
	ld (hl),e	
	nop	
	nop	
	nop	
	nop	
	ld bc,sub_1017h
	ld h,d	
	ld (hl),l	
	ld h,(hl)	
	ld h,(hl)	
	ld h,l	
	ld (hl),d	
	nop	
	nop	
	inc bc	
	scf	
	djnz $+114
l1072h:
	ld (hl),l	
	ld (hl),h	
	ld h,e	
	nop	
	nop	
	nop	
	nop	
	ld bc,sub_1021h
