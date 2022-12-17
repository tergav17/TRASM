/*
 * asm.c 
 *
 * assembler guts
 */
#include "asm.h"
#include "sio.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>


/*
 * there are a number of global variables to reduce the amount of data being passed
 */

/* token buffer */
char token_buf[TOKEN_BUF_SIZE];
char token_cache[TOKEN_BUF_SIZE];

/* current assembly address and index */
uint16_t asm_address;
uint16_t asm_index;

/* current pass */
char asm_curr_pass;

/* the expression evaluator requires some larger data structures, lets define them */

/* value stack */
uint16_t exp_vstack[EXP_STACK_DEPTH];

/* expression stack */
char exp_estack[EXP_STACK_DEPTH];

/* heap top */
int heap_top;

/* head of symbol list */
struct symbol *sym_table;

/* since this is suppose to work like the assembly version, we will preallocate a heap */
char heap[HEAP_SIZE];

/*
 * allocates memory from the heap
 *
 * size = number of bytes to allocate
 */
void *asm_alloc(int size)
{
	int old_pointer;
	
	old_pointer = heap_top;
	heap_top += size;
	
	return (void *) &heap[old_pointer];
}

/*
 * resets the top of the heap to the bottom
 */
void asm_reset()
{
	heap_top = 0;
	sym_table = NULL;
	
	// allocate empty table
	sym_table = (struct symbol *) asm_alloc(sizeof(struct symbol));
}

/*
 * moves the contents of token_buf into token_cache
 */
void asm_token_cache()
{
	int i;
	
	for (i = 0; i < TOKEN_BUF_SIZE; i++)
		token_cache[i] = token_buf[i];
}


/*
 * skips past all of the white space to a token
 */
void asm_wskip()
{
	char comment;
	
	comment = 0;
	while ((sio_peek() <= ' ' || sio_peek() == ';' || comment) && sio_peek() != '\n' && sio_peek() != -1)
		if (sio_next() == ';') comment = 1;
}

/*
 * prints out an error message and exits
 *
 * msg = error message
 */
void asm_error(char *msg)
{
	sio_status();
	printf(": %s\n", msg);
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
char asm_token_read() 
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
			if (i < TOKEN_BUF_SIZE - 1)
				token_buf[i++] = c;
			
			sio_next();
			c = sio_peek();
		}
		token_buf[i] = 0;
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
 * consumes a char that as the next symbol, and then finds the next symbol ignoring new lines
 */
void asm_lineable(char c)
{
	char tok;
	
	tok = asm_token_read();
	
	if (tok != c)
		asm_error ("unexpected character");
	
	while (sio_peek() == '\n')
		asm_token_read();
}

/*
 * helper function for number parsing, returns radix type from character
 *
 * r = radix identifier
 * returns radix type, or 0 if not a radix
 */
char asm_classify_radix(char r)
{
	if (r == 'b' || r == 'B') return 2;
	if (r == 'o' || r == 'O') return 8;
	if (r == 'x' || r == 'X' || r == 'h' || r == 'H') return 16;
	return 0;
}

/*
 * another helper function, this time to take a decimal / hex char and convert it
 *
 * in = char to convert to number
 * returns number, of -1 if failed
 */
int asm_char_parse(char in)
{
	if (in >= '0' && in <= '9') return in - '0';
	else if (in >= 'A' && in <= 'F') return (in - 'A') + 10;
	else if (in >= 'a' && in <= 'f') return (in - 'a') + 10;
	return -1;
}

/*
 * attempts to parse a number into an unsigned 16 bit integer
 *
 * in = pointer to string
 * returns actual value of number
 */
uint16_t asm_num_parse(char *in)
{
	int num_start, num_end, i;
	uint16_t out;
	char radix; // 0 = ?, 2 = binary, 8 = octal, 10 = decimal, 16 = hex
	
	// default is base 10
	radix = 10;
	
	// first skip through any leading zeros, and set octal if there is some
	for (num_start = 0; in[num_start] == '0'; num_start++)
		radix = 8;
	
	// lets also find the end while we are at it
	for (num_end = 0; in[num_end] != 0; num_end++);
	
	// check and see if there is a radix identifier here
	if ((i = asm_classify_radix(in[num_start]))) {
		radix = i;
		num_start++;
	} else {
		// lets check at the end too
		if ((i = asm_classify_radix(in[num_end - 1]))) {
			radix = i;
			num_end--;
		}
	}
	
	// now to parse
	out = 0;
	for (; num_start < num_end; num_start++) {
		i = asm_char_parse(in[num_start]);
		
		// error checking
		if (i == -1) asm_error("unexpected character in numeric");
		if (i >= radix) asm_error("radix mismatch in numeric");
		
		out = (out * radix) + i;
	}
	
	return out;
}

/*
 * fetches the symbol
 *
 * sym = pointer to symbol name
 * returns pointer to found symbol, or null
 */
struct symbol *asm_sym_fetch(struct symbol *parent, char *sym)
{
	struct symbol *entry;
	int i;
	char equal;
	
	// search for the symbol
	entry = parent->next;
	
	while (entry) {
		
		// compare strings
		equal = 1;
		for (i = 0; i < 9 && entry->name[i] != 0; i++)
			if (entry->name[i] != sym[i]) equal = 0;
		
		if (equal) return entry;
		
		entry = entry->next;
	}
	
	
	return NULL;
}

/*
 * defines or redefines a symbol
 *
 * sym = symbol name
 * type = symbol type (0 = undefined, 1 = relocatable, 2 = static)
 * parent = parent name
 * value = value of symbol
 */
void asm_sym_update(struct symbol *table, char *sym, char type, char *parent, uint16_t value)
{
	struct symbol *entry;
	int i;
	
	entry = asm_sym_fetch(table, sym);
	
	if (!entry) {
		entry = sym_table;
		
		// get the last entry in the table;
		while (entry->next)
			entry = entry->next;
		
		entry->next = (struct symbol *) asm_alloc(sizeof(struct symbol));
		entry = entry->next;
		entry->parent = NULL;
		entry->size = 0;
		
		// copy name
		for (i = 0; i < 8 && sym[i] != 0; i++)
			entry->name[i] = sym[i];
		entry->name[i] = 0;
			
	}
	
	// update the symbol
	entry->type = type;
	if (parent != NULL) {
		entry->parent = asm_sym_fetch(sym_table, parent);
		if (entry->parent == NULL)
			asm_error("parent types does not exist");
	}
	entry->value = value;
}

/*
 * converts an escaped char into its value
 *
 * c = char to escape
 * returns escaped value
 */
char asm_escape_char(char c)
{
	switch (c) {
		case 'a':
			return 0x07;
			
		case 'b':
			return 0x08;
			
		case 'e':
			return 0x1B;
			
		case 'f':
			return 0x0C;
			
		case 'n':
			return 0x0A;
			
		case 't':
			return 0x09;
			
		case 'v':
			return 0x0B;
		
		case '\\':
			return 0x5C;

		case '\'':
			return 0x27;
			
		case '\"':
			return 0x22;
			
		case '\?':
			return 0x3F;
			
		default:
			return 0;
	}
}

/*
 * pops a value off the estack and evaluates it in the vstack
 *
 * eindex = pointer to expression index
 * vindex = pointer to value index
 */
void exp_estack_pop(int *eindex, int *vindex)
{
	uint16_t a,b, res;
	char op;
	
	// check if expression stack is empty
	if (!(*eindex)) asm_error("expression stack depletion");
	
	// pop off estack
	op = exp_estack[--*eindex];
	
	// attempt to pop out two values from the value stack
	if (*vindex < 2) asm_error("value stack depletion");
	
	b = exp_vstack[--*vindex];
	a = exp_vstack[--*vindex];
	
	switch (op) {
		case '!':
			res = a | ~b;
			break;
		
		case '+':
			res = a + b;
			break;
			
		case '-':
			res = a - b;
			break;
			
		case '*':
			res = a * b;
			break;
			
		case '/':
			if (b == 0) {
				if (asm_curr_pass == 0) res = 0;
				else asm_error("zero divide");
			} else
				res = a / b;
			break;
			
		case '%':
			res = a % b;
			break;
			
		case '>':
			res = a >> b;
			break;
			
		case '<':
			res = a << b;
			break;
			
		case '&':
			res = a & b;
			break;
		
		case '^':
			res = a ^ b;
			break;
			
		case '|':
			res = a | b;
			break;
			
		case '(':
			asm_error("unexpected '('");
		
		default:
			res = 0;
			break;
	}
	
	// push into stack
	exp_vstack[(*vindex)++] = res;
}

/*
 * pushes a expression onto the estack
 *
 * eindex = pointer to expression idnex
 * op = expression to push
 */
void exp_estack_push(int *eindex, char op)
{
	if (*eindex >= EXP_STACK_DEPTH) asm_error("expression stack overflow");
	exp_estack[(*eindex)++] = op;
}

/*
 * pushes a value onto the vstack
 *
 * vindex = pointer to value index
 * val = value to push
 */
void exp_vstack_push(int *vindex, uint16_t val)
{
	if (*vindex >= EXP_STACK_DEPTH) asm_error("value stack overflow");
	exp_vstack[(*vindex)++] = val;
}

/*
 * returns the precedence of a specific token
 *
 * tok = token
 * returns precedence of token
 */
int asm_precedence(char tok)
{
	switch (tok) {
		case '!':
			return 1;
			
		case '+':
		case '-':
			return 2;
			
		case '*':
		case '/':
		case '%':
			return 3;
			
		case '>':
		case '<':
			return 4;
			
		case '&':
			return 5;
		
		case '^':
			return 6;
			
		case '|':
			return 7;
			
		case '(':
			return 0;
			
		default:
			return 99;
	}
}

/*
 * checks to see if there is a left parathesis in the expression stack
 *
 * size = size of stack
 * returns 1 if true, otherwise 0
 */
char exp_estack_has_lpar(int size) 
{
	int i;
	
	for (i = 0; i < size; i++)
		if (exp_estack[i] == '(') return 1;
	
	return 0;
}

/*
 * evaluates an expression that is next in the token queue
 *
 * result = pointer where result will be placed in
 * returns status 0 = unresolved, 1 = relocatable, 2 = static
 */
char asm_evaluate(uint16_t *result)
{
	char tok, op, reloc;
	uint16_t num;
	struct symbol *sym;
	int vindex, eindex;
	
	// reset indicies
	vindex = eindex = 0;
	
	// check for relocation
	if (sio_peek() == '*') {
		asm_token_read();
		reloc = 1;
	} else reloc = 2;
	
	while (1) {
		tok = asm_token_read();
		
		if (tok == 'a') {
			// it is a symbol
			op = 0;
			sym = asm_sym_fetch(sym_table, token_buf);
			if (sym) {
				if (sym->type < reloc) reloc = sym->type;
				num = sym->value;
			} else {
				reloc = 0;
				num = 0;
			}
		} else if (tok == '0') {
			// it is a numeric
			op = 0;
			num = asm_num_parse(token_buf);
		} else if (tok == '\'') {
			// it is a char
			op = 0;
			
			// escape character
			if (sio_peek() == '\\') {
				sio_next();
				num = asm_escape_char(sio_next());
				
				if (!num) asm_error("unknown escape");
			} else {
				num = sio_next();
			}
			
			if (asm_token_read() != '\'')
				asm_error("expected \'");
		} else {
			// it is a token (hopefully mathematic)
			op = -1;
			
			if (tok == '+' || tok == '-' || tok == '*' || tok == '/' || 
				tok == '&' || tok == '|' || tok == '%' || tok == '!' ||
				tok == '^' || tok == '(' || tok == ')') op = tok;
				
			if (tok == '>' || tok == '<') {
				if (tok != sio_peek()) op = -1;
				else op = tok;
				
				asm_token_read();
			}
			
			if (op == -1) asm_error("unknown token in expression");
		}
		
		// now lets handle the token
		if (op != ')' && op != '(' && op) {
			// handle operators
			
			// pop off anything in the stack that is of higher precedence
			while (eindex && asm_precedence(op) <= asm_precedence(exp_estack[eindex - 1]))
				exp_estack_pop(&eindex, &vindex);
			
			exp_estack_push(&eindex, op);
		} else if (op == '(') {
			// handle left parathesis 
			exp_estack_push(&eindex, '(');
		} else if (op == ')') {
			if (!exp_estack_has_lpar(eindex))
				asm_error("unexpected ')'");
			
			while (exp_estack[eindex - 1] != '(')
				exp_estack_pop(&eindex, &vindex);
			
			// pop the '(' too
			eindex--;
		} else {
			// handle numbers
			exp_vstack_push(&vindex, num);
		}
		
		// check for ending conditions
		tok = sio_peek();
		if (tok == ',' || tok == '\n' || tok == -1) break;
		if (tok == ')' && !exp_estack_has_lpar(eindex)) break;
			
	}
	
	while (eindex) exp_estack_pop(&eindex, &vindex);
	
	if (vindex != 1) asm_error("value stack overpopulation");
	
	*result = exp_vstack[0];
	
	return reloc;
}

/*
 * emits a number of bytes into assembly output
 * no bytes emitted on first pass, only indicies updated
 *
 * s = pointer to byte array
 * n = number of bytes
 */
void asm_emit(uint8_t *s, int n)
{
	int i;
	
	asm_address += n;
	asm_index += n;
	
	for (i = 0; i < n; i++) {
		printf("emitting: %02x\n", s[i]);
	}
}

/*
 * grabs the size of a type, and returns its parent symbol chain
 *
 * type = name of type
 * result = size of symbol, 0 if does not exist
 * returns parent symbol chain
 */
struct symbol *asm_type_size(char *type, uint16_t *result)
{
	struct symbol *sym;
	
	// built in types
	if (!strcmp(type, "byte")) {
		*result = 1;
		return NULL;
	}
	
	// else, look in the symbol table
	sym = asm_sym_fetch(sym_table, type);
	
	if (sym) {
		*result = sym->size;
		return sym;
	}
	
	// can't find
	*result = 0;
	return NULL;
}

/*
 * emits a string found in the char stream
 */
void asm_string_emit()
{
	char c, state;
	int radix, length;
	uint8_t decode, num;
	
	// zero state, just accept raw characters
	state = 0;
	
	sio_next();
	while (1) {
		c = sio_next();
		
		// we are done (maybe)
		if (c == -1) break;
		if (c == '"') {
			if (state != 1) {
				if (state == 3) {
					asm_emit(&decode, 1);
				}
				
				break;
			}
		}
		
		// just emit the char outright
		if (!state) {
			// sets the state to 1
			if (c == '\\') 
				state = 1;
			else 
				asm_emit((uint8_t *) &c, 1);
			
			
		} else if (state == 1) {
			// escape character
			decode = asm_escape_char(c);
			
			// simple escape
			if (decode) {
				asm_emit(&decode, 1);
				state = 0;
			} else if (asm_num(c)) {
				state = 3;
				radix = 8;
				length = 3;
			} else if (c == 'x') {
				state = 2;
				radix = 16;
				length = 2;
			} else {
				asm_error("unknown escape");
			}
		}
		
		if (state == 3) {
			// numeric parsing
			num = asm_char_parse(c);
			
			if (num == -1) asm_error("unexpected character in numeric");
			if (num >= radix) asm_error("radix mismatch in numeric");
			
			decode = (decode * radix) + num;
			
			num = asm_classify_radix(sio_peek());
			length--;
			
			// end the parsing
			if (length < 1 || num == -1 || num >= radix) {
				state = 0;
				asm_emit(&decode, 1);
			}
		}
		
		// this is to consume the 'x' identifier 
		if (state == 2) state = 3;
	}
	
	// make sure we don't land in whitespace
	asm_wskip();
}

/*
 * fills a region with either zeros or undefined allocated space
 *
 * count = number of bytes to fille
 */
void asm_fill(uint16_t space)
{
	while (space--) asm_emit((uint8_t *) "\x00", 1);
}


/*
 * recursive function to do type-based definitions
 */
void asm_define_type(struct symbol *parent, uint16_t size)
{
	struct symbol *sym;
	char tok;
	uint16_t base;
	
	if (!parent)
		asm_error("type has no parent");
	
	base = asm_address;
	
	// get the first field
	asm_lineable('{');
	
	sym = parent;
	while (sym) {
		if (asm_address > base + sym->value)
			asm_error("field domain overrun");
		
		tok = sio_peek();
		if (tok == '"') {
			// emit the string
			asm_string_emit();
			
		} else if (tok == '{') {
			// emit the type
			asm_define_type(sym->parent, size);
			
		} else {
			tok = asm_evaluate(&value);
			
			// this needs fixed
			
			if (!tok) {
				value = 0;
			} else if (tok == 1) {
				// relocation marking if size=2
			}
			
			b = value & 0xFF;
			asm_emit(&b, 1);
			if (size > 1) {
				b = (value & 0xFF00) >> 8;
				asm_emit(&b, 1);
			}
		}
		
		if (sym->next) {
			asm_lineable(',');
		}
	}
	
	asm_address = base;
}

/*
 * parses a definition out of the token queue, and emits it
 *
 * type = type of data
 * count = number of data structures created
 */
void asm_define(char *type, uint16_t count)
{
	char tok;
	int i;
	struct symbol *parent;
	uint16_t size, addr, value;
	uint8_t b;
	
	// get the symbol type
	parent = asm_type_size(type, &size);
	
	if (!size) asm_error("not a type");
	
	// record current address
	addr = asm_address;
	
	i = 0;
	while (sio_peek() != '\n' && sio_peek() != -1) {
		tok = sio_peek();
		if (tok == '"') {
			// emit the string
			asm_string_emit();
			
		} else if (tok == '{') {
			// emit the type
			asm_define_type(parent, size);
			
		} else {
			tok = asm_evaluate(&value);
			
			if (!tok) {
				value = 0;
			} else if (tok == 1) {
				// relocation marking if size=2
			}
			
			b = value & 0xFF;
			asm_emit(&b, 1);
			if (size > 1) {
				b = (value & 0xFF00) >> 8;
				asm_emit(&b, 1);
			}
		}
		
		// see how many elements we emitted, and align to size
		while (asm_address > addr) {
			addr += size;
			i++;
		}
		asm_fill(addr - asm_address);
		


		if (sio_peek() != '\n' && sio_peek() != -1) asm_lineable(',');
	}
}

/*
 * attempts to assemble an instruction assuming a symbol has just been tokenized
 *
 * in = pointer to string
 * returns 0 if an instruction is not matched, 1 if it is
 */
char asm_instr(char *in)
{
	uint16_t result;
	
	if (!strcmp(in, "nop")) {
		asm_emit((uint8_t *) "\x00", 1);
		return 1;
	} else if (!strcmp(in, "test")) {
		if (asm_evaluate(&result))
			printf("Exp: %d\n", result);
		return 1;
	}
	
	return 0;
}

/*
 * consumes an end of line
 */
void asm_eol()
{
	char tok = asm_token_read();
	if (tok != 'n' && tok != -1)
		asm_error("expected end of line");
}

/*
 * perform assembly functions
 * pass 1 (pass = 0) will generate the symbol table
 * pass 2 (pass = 1) will produce code
 */
void asm_pass(int pass)
{
	char tok, type;
	uint16_t res;

	asm_curr_pass = pass;

	// assembler start at 0;
	asm_address = asm_index = 0;
	
	// general line input stuff
	while (1) {
		// Read the next 
		tok = asm_token_read();
		if (tok == -1) break;
		
		// command read
		if (tok == '.') {
			tok = asm_token_read();
			
			if (tok != 'a')
				asm_error("expected directive");
			
			if (!strcmp(token_buf, "def")) {
				tok = asm_token_read();
				if (tok == 'a')
					asm_define(token_buf, 0);
				else
					asm_error("invalid type");
			}
		}
		
		// symbol read
		else if (tok == 'a')  {
			
			// try to get the type of the symbol
			if (asm_instr(token_buf)) {
				// it's an instruction
				asm_eol();
			} else if (sio_peek() == '=') {
				// it's a symbol definition
				asm_token_cache();
				asm_token_read();
				
				// evaluate the expression
				type = asm_evaluate(&res);
				
				// set the new symbol
				asm_sym_update(sym_table, token_cache, type, NULL, res);
				asm_eol();
			} else if (sio_peek() == ':') {
				// it's a label
				
				// set the new symbol
				asm_sym_update(sym_table, token_buf, 1, NULL, asm_address);
				asm_token_read();
				asm_eol();
			} else {
				printf("Symbol: %s\n", token_buf);
			}
		} 
		
		else if (tok == '0') {
			// numeric read
			printf("Numeric: %d\n", asm_num_parse(token_buf));
		}
	}
}