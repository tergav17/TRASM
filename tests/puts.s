; Put string routine

.globl puts
puts:
	ld	a,(hl)
	or	a
	ret	z
	; output a
	inc	hl
	jp	puts
