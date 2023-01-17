; Part 1 of hello world test

.extern puts

.text
	ld	hl, hello_s
	call	puts
	ret

.data
	.defl byte hello_s "Hello, world\n\0"
