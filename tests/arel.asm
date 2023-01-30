; z80dasm 1.1.6
; command line: z80dasm -l -g 0x1000 -o arel.asm arel.out

	org	01000h

	ld hl,01019h
	call sub_1007h
	ret	
sub_1007h:
	ld a,(hl)	
	or a	
	ret z	
	call sub_1011h
	inc hl	
	jp sub_1007h
sub_1011h:
	ld b,001h
	call 00ff5h
	ret	
	add hl,de	
	djnz $+74
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
