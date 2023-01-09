; relocation encoding test program
.data

0:	.def word 1f
	.def byte[0x0F]
1:	.def word 0b

.text
0:	.def word 0b