; instruction tests

	call m,test
	rst 0x30
test:
	.def byte 0x69,0b0