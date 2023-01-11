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

data_v =	1
text_v = 	1

; text_v should emit, then data_v

.data

	.def byte	0
1:	.def byte	"Hello :", data_v, 0
2:	.def word_t	2b

.if text_v

	.def byte	"text_v is true!",0
	
	.if data_v

		.def byte	"data_v is also true!",0
	.endif
.endif
	

.text

	.def byte	$foobar.b.low
testlbl:
	.def word_t	1b,1f
	
1: