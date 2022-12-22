; test program

.type foobar {
	byte a,
	word b,
	byte c
}

.type foobar2 {
	byte bar,
	foobar foo
	
}

.def byte 0

.defl foobar bazinga {
	0x69, 0x420, 0b1
}

	test foobar2.foo.b