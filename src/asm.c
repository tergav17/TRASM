/*
 * asm.c 
 *
 * assembler guts
 */
#include "asm.h"
#include "sio.h"

#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdlib.h>


/*
 * there are a number of global variables to reduce the amount of data being passed
 */

/* assembly buffer */
char asm_buf[32];

/* current assembly address and index */
uint16_t asm_address;
uint16_t asm_index;

/* current pass */
char asm_curr_pass;

/*
 * skips past all of the white space to a token
 */
void asm_wskip()
{
	while (sio_peek() <= ' ' && sio_peek() != '\n' && sio_peek() != -1)
		sio_next();
}

void asm_error(char *msg)
{
	printf("error: %s\n", msg);
	exit(1);
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
	
	if (out == 'a' || out == '0') {
			// scan in the buffer if needed
		i = 0;
		while (asm_num(c) || asm_alpha(c)) {
			if (i < sizeof(asm_buf) - 1)
				asm_buf[i++] = c;
			
			sio_next();
			c = sio_peek();
		}
		asm_buf[i] = 0;
	} else if (out == ';') {
			// if comment, just skip everything till the next line break
			while (sio_peek() != '\n' && sio_peek() != -1)
				sio_next();
			
			out = sio_next();
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
 * emits a number of bytes into assembly output
 * no bytes emitted on first pass, only indicies updated
 *
 * s = pointer to byte array
 * n = number of bytes
 */
void asm_emit(char *s, int n)
{
	asm_address += n;
	asm_index += n;
}

/*
 * attempts to assemble an instruction assuming a symbol has just been tokenized
 *
 * returns 0 if an instruction is not matched, 1 if it is
 */
char asm_instr()
{
	if (!strcmp(asm_buf, "nop")) {
		asm_emit("\x00", 1);
		return 1;
	}
	
	return 0;
}

/*
 * perform assembly functions
 * pass 1 (pass = 0) will generate the symbol table
 * pass 2 (pass = 1) will produce code
 */
void asm_pass(int pass)
{
	char sym;

	asm_curr_pass = pass;

	// assembler start at 0;
	asm_address = asm_index = 0;
	
	// general line input stuff
	while (1) {
		// Read the next 
		sym = asm_read_token();
		if (sym == -1) break;
		
		if (sym == 'a')  {
			// symbol read
			if (asm_instr()) {
				// its an instruction
				sym = asm_read_token();
				if (sym != 'n')
					asm_error("expected end of line");
			} else {
				printf("Symbol: %s\n", asm_buf);
			}
		} else if (sym == '0') {
			// numeric read
			printf("Numeric: %s\n", asm_buf);
		}
		else printf("Read: %c\n", sym);
	}
}