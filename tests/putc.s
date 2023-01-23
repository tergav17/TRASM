; Put char routine

.text
.globl putc
putc:
	out	(0),a
	ret
