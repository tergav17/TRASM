; test program


.type word_t {
	byte low,
	byte high
}

.type foobar {
	byte a,
	word_t b,
	byte c
}

data_v =	0x69
text_v = 	0x42

; text_v should emit, then data_v

.data

.def byte	data_v

.text

.def byte	text_v