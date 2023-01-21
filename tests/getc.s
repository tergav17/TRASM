; Put string routine

.text
.globl getc
getc:
	in	a,(0)
	ret
