/*
 * asm.c 
 *
 * assembler guts
 */
#include "asm.h"
#include "sio.h"

#include <stdio.h>

/* assembly buffer */
char asm_buf[32];

/*
 * skips past all of the white space to a token
 */
void asm_wskip()
{
	while (sio_peek() <= ' ' && sio_peek() != '\n' && sio_peek() != -1)
		sio_next();
}

/*
 * tests if a character is a alpha (aA - zZ or underscore)
 *
 * in = character to test
 * returns true (1) or false (0)
 */
char asm_alpha(char in)
{
	return (in >= 'A' && in <= 'Z') || (in >= 'a' && in <= 'z') || in == '_';
}

/*
 * tests if a character is a alpha
 *
 * in = character to test
 * returns true (1) or false (0)
 */
char asm_num(char in)
{
	return (in >= '0' && in <= '9');
}

/*
 * reads the next token in from the source, buffers if needed, and returns type
 * white space will by cycled past, both in front and behind the token
 */
char asm_read_token() 
{
	char c, out;
	int i;
	
	// skip all leading white space
	asm_wskip();
	
	// peek and check type
	out = c = sio_peek();
	if (asm_alpha(c))
		out = 'a';
	else if (asm_num(c))
		out = '0';
	else
		out = c;
	
	// scan in the buffer if needed
	if (out == 'a' || out == '0') {
		i = 0;
		while (asm_num(c) || asm_alpha(c)) {
			if (i < sizeof(asm_buf) - 1)
				asm_buf[i++] = c;
			
			sio_next();
			c = sio_peek();
		}
		asm_buf[i] = 0;
	} else {
		sio_next();
	}
	
	// correct for new lines
	if (out == '\n') out = 'n';
	
	// skip more whitespace 
	asm_wskip();
	
	return out;
}

/*
 * perform assembly functions
 * pass 1 (pass = 0) will generate the symbol table
 * pass 2 (pass = 1) will produce code
 */
void asm_pass(int pass)
{
	char sym;
	
	// keep taking in tokens 
	while (1) {
		sym = asm_read_token();
		
		if (sym == -1) break;
		
		if (sym == 'a' || sym == '0') 
			printf("Symbol: %s\n", asm_buf);
		else printf("Read: %c\n", sym);
	}
}