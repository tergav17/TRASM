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

bruh = 0x1069

.def foobar2 {
	0b1100,
	{1, 2, 3}
}

test	foobar2.foo.c