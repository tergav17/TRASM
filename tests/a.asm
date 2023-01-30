; z80dasm 1.1.6
; command line: z80dasm -l -g 0x0000 -o a.asm a.out

	org	00000h

l0000h:
	jr l0010h
	inc bc	
	nop	
	nop	
sub_0005h:
	jp l0000h
	nop	
	nop	
	daa	
	nop	
	scf	
	nop	
	ld d,a	
	nop	
l0010h:
	ld hl,l0029h
	call sub_0017h
	ret	
sub_0017h:
	ld a,(hl)	
	or a	
	ret z	
	call sub_0021h
	inc hl	
	jp sub_0017h
sub_0021h:
	ld b,001h
	call sub_0005h
	ret	
	add hl,hl	
	nop	
l0029h:
	ld c,b	
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
	dec b	
	nop	
