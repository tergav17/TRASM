; Put string routine

.text
.globl puts
puts:
	ld	a,(hl)
	or	a
	ret	z
	out	(0),a
	inc	hl
	jp	puts

.bss
.globl buffer
	.defl byte[32] buffer
