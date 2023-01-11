; relocation encoding test program
.data

0:		.def word 1f
		.def byte[0x0A]
1:		.def word 0b

.text
		.def byte[1]
label:	.def word label
		.def byte[1]
	
.bss
		.defl byte[64] buffer
		
.globl label