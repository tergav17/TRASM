; relocation encoding test program
.data

0:	.def word 1f
	.def byte[0xFF]
1:	.def word 0b

.text
	.def byte[1]
0:	.def word 0b
	.def byte[1]