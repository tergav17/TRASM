; test program

.type foobar {
	byte a,
	word b,
	byte c
}

.type foobar2 {
	foobar foo,
	byte bar
}

.def word a, b, c

.def foobar {
	0, 1, 2
}

;.def foobar2 {
;	{
;		0x12,
;		0x3456, 
;		0x78
;	},
;	0
;}