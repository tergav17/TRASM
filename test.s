label_a:
	nop
	ld	a,b
	foobar
	
label_b: ; comment here
	jp	label_a
; another comment 

; one last comment
	ld b,08
