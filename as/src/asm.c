/*
 * asm.c 
 *
 * assembler guts
 * i really should have broken this up, but it will all be rewritten in assembly later...
 */
#include "asm.h"
#include "sio.h"

// instruction table
#include "isr.h"

#include <stdio.h>
#include <stdlib.h>

/*
 * there are a number of global variables to reduce the amount of data being passed
 */

/* token buffer */
char token_buf[TOKEN_BUF_SIZE];
char sym_name[TOKEN_BUF_SIZE];

/* current assembly address */
uint16_t asm_address;

/* segment tops */
uint16_t text_top;
uint16_t data_top;
uint16_t bss_top;

uint16_t text_size;

/* current pass */
char asm_pass;

/* current segment */
char asm_seg;

/* the expression evaluator requires some larger data structures, lets define them */

/* value stack */
struct tval exp_vstack[EXP_STACK_DEPTH];

/* expression stack */
char exp_estack[EXP_STACK_DEPTH];

/* head of symbol table */
struct symbol *sym_table;

/* head of local table */
struct local *loc_table;

/* counts how many locals have been encountered this pass */
int loc_cnt;

/* head of global table */
struct global *glob_table;

/* head of relocation tables */
struct header textr;
struct header datar;

/* record keeping */
uint16_t reloc_rec;
uint16_t glob_rec;

/* telemetry stuff */
int sym_count;
int loc_count;
int glob_count;
int reloc_count;

/* extern number */
uint8_t extn;
/*
 * checks if a string is equal
 * string a is read as lower case
 *
 * a = pointer to string a
 * b = pointer to string b
 */
char asm_sequ(char *a, char *b) {
	char lower;
	
	while (*b) {
		lower = *a;
		if (lower >= 'A' && lower <= 'Z')
			lower += 'a'-'A';
		
		if (lower != *b)
			return 0;
		
		a++;
		b++;
	}
	
	return *a==*b;
	
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
	sio_close();
	exit(1);
}


/*
 * allocates memory from the heap
 * since no de-allocation is needed, data could just be appended to the heap
 *
 * size = number of bytes to allocate
 */
void *asm_alloc(int size)
{
	return (void *) malloc(size);
}

/*
 * creates a new reloc struct and inits it
 *
 * returns new object
 */
struct reloc *asm_alloc_reloc()
{
	struct reloc *new;
	int i;
	
	// allocate start of relocation table
	reloc_count++;
	new = (struct reloc *) asm_alloc(sizeof(struct reloc));
	for (i = 0; i < RELOC_SIZE; i++) new->toff[i].off = 255;
	new->next = NULL;
	
	return new;
}

/*
 * moves the contents of token_buf into token_cache
 */
void asm_token_cache(char *token_cache)
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
 * expects a specific symbol
 * some symbols have special cases for ignoring trailing or leading line breaks
 *
 * c = symbol to expect
 */
void asm_expect(char c)
{
	char tok;
	
	if (c == '}') {
		while (sio_peek() == '\n')
			asm_token_read();
	}
	
	tok = asm_token_read();
	
	if (tok != c) {
		asm_error ("unexpected character");
	}
	
	if (c == '{' || c == ',') {
		while (sio_peek() == '\n')
			asm_token_read();
	}
}

/*
 * consumes an end of line
 */
void asm_eol()
{
	char tok;
	
	tok = asm_token_read();
	if (tok != 'n' && tok != -1)
		asm_error("expected end of line");
}

/*
 * skips to and consumes an end of line
 */
void asm_skip()
{
	char tok;
	
	do {
		tok = asm_token_read();
	} while (tok != 'n' && tok != -1);
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
 * parent = parent structure to search
 * sym = pointer to symbol name
 * returns pointer to found symbol, or null
 */
struct symbol *asm_sym_fetch(struct symbol *table, char *sym)
{
	struct symbol *entry;
	int i;
	char equal;
	
	if (!table)
		return NULL;
	
	// search for the symbol
	entry = table->parent;
	
	while (entry) {
		
		// compare strings
		equal = 1;
		for (i = 0; i < SYMBOL_NAME_SIZE; i++) {
			if (entry->name[i] != sym[i]) equal = 0;
			if (!entry->name[i]) break;
		}
		
		if (equal) return entry;
		
		entry = entry->next;
	}
	
	
	return NULL;
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
	if (asm_sequ(type, "byte")) {
		*result = 1;
		return NULL;
	}
	if (asm_sequ(type, "word")) {
		*result = 2;
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
 * defines or redefines a symbol
 *
 * table = table to add to
 * sym = symbol name
 * type = symbol type (0 = undefined, 1 = text, 2 = data, 3 = bss, 4 = absolute, 5+ = external)
 * parent = parent name
 * value = value of symbol
 */
struct symbol *asm_sym_update(struct symbol *table, char *sym, char type, struct symbol *parent, uint16_t value)
{
	struct symbol *entry;
	int i;
	
	entry = asm_sym_fetch(table, sym);
	
	if (!entry) {
		if (table->parent) {
			entry = table->parent;
			
			// get the last entry in the table;
			while (entry->next)
				entry = entry->next;
			
			sym_count++;
			entry->next = (struct symbol *) asm_alloc(sizeof(struct symbol));
			entry = entry->next;
		} else {
			sym_count++;
			table->parent = (struct symbol *) asm_alloc(sizeof(struct symbol));
			entry = table->parent;
		}
		
		entry->next = NULL;
		entry->parent = NULL;
		entry->size = 0;
		
		// copy name
		for (i = 0; i < SYMBOL_NAME_SIZE-1 && sym[i] != 0; i++)
			entry->name[i] = sym[i];
		entry->name[i] = 0;
			
	}
	
	// update the symbol
	entry->type = type;
	if (parent != NULL) {
		entry->parent = parent->parent;
	}
	entry->value = value;
	
	return entry;
}

/*
 * resets all allocation stuff
 */
void asm_reset()
{	
	
	sym_table = NULL;
	loc_table = NULL;
	glob_table = NULL;
	
	// allocate empty table
	sym_table = (struct symbol *) asm_alloc(sizeof(struct symbol));
	sym_table->parent = NULL;
	
	asm_sym_update(sym_table, "sys", 1, NULL, 0x0005);
	asm_sym_update(sym_table, "header", 1, NULL, 0x0000);
	
	// allocate relocation tables
	textr.last = 0;
	textr.index = 0;
	textr.head = asm_alloc_reloc();
	textr.tail = textr.head;
	datar.last = 0;
	datar.index = 0;
	datar.head = asm_alloc_reloc();
	datar.tail = datar.head;

	
	// count memory consumption
	sym_count = 0;
	loc_count = 0;
	glob_count = 0;
	reloc_count = 0;
	
	// externs start at 5
	extn = 5;
}

/*
 * appends a local symbol to the local table
 *
 * label = label # (0-9)
 * type = symbol segment (text, data, or bss)
 * value = value of symbol
 */
void asm_local_add(uint8_t label, uint8_t type, uint16_t value)
{
	struct local *new, *curr;
	
	// alloc the new local symbol
	loc_count++;
	new = (struct local *) asm_alloc(sizeof(struct local));
	new->label = label;
	new->type = type;
	new->value = value;
	
	// append to local table
	if (loc_table) {
		curr = loc_table;
		while (curr->next)
			curr = curr->next;
		
		curr->next = new;
	} else
		loc_table = new;
}

/*
 * fetches a local symbol
 *
 * index = how many local indicies have been counted during pass
 * label = label # (0-9)
 * dir = direction (0 = backwards, 1 = forwards)
 */
char asm_local_fetch(uint16_t *result, int index, uint8_t label, char dir)
{
	struct local *curr, *last;
	
	curr = loc_table;
	
	// iterate through list
	last = NULL;
	while (curr) {
		
		// label match
		if (curr->label == label) {
			if (index) {
				last = curr;
			} else break;
		}
		
		if (index) index--;
		curr = curr->next;
	}
	
	if (dir)
		last = curr;
	
	*result = 0;
	if (last) {
		*result = last->value;
		return last->type;
	}
	return 0;
}

/*
 * adds a symbol to the global table
 *
 * sym = symbol to add
 */
void asm_glob(struct symbol *sym) 
{
	struct global *curr, *new;
	
	glob_count++;
	glob_rec++;
	new = (struct global *) asm_alloc(sizeof(struct global));
	new->symbol = sym;
	new->next = NULL;
	
	curr = NULL;
	if (glob_table) {
		curr = glob_table;
		
		while (1) {
			// if the symbol already is glob, just ignore it
			if (curr->symbol == sym)
				return;
			
			if (curr->next) {
				curr = curr->next;
			} else {
				curr->next = new;
				return;
			}
		}
	} else {
		glob_table = new;
	}
}

/*
 * adds an address into a relocation table, extending it if needed
 *
 * tab = reloc table
 * target = address to add
 */
void asm_reloc(struct header *tab, uint16_t addr, uint8_t type)
{
	uint16_t diff;
	uint8_t i, next;
	
	
	if (addr < tab->last) 
		asm_error("backwards reloc");
	
	diff = addr - tab->last;
	
	// grab the initial index
	i = tab->index;
	do {
		if (diff >= 254) {
			diff -= 254;
			next = 254;
		} else {
			next = diff;
		}
		
		// set the type and value
		tab->tail->toff[i].type = type;
		tab->tail->toff[i++].off = next;
	
		// see if another table is needed
		if (i == RELOC_SIZE) {
			i = 0;
			tab->tail->next = asm_alloc_reloc();
			tab->tail = tab->tail->next;
		}
	} while (next == 254);

	// set the index back
	tab->index = i;
	tab->last = addr;
	reloc_rec++;
}


/*
 * outputs a relocation table to a.out
 *
 * tab = relocation table
 */
void asm_reloc_out(struct reloc *tab, uint16_t base)
{
	int i;
	
	i = 0;
	while (tab) {
		base += tab->toff[i].off;
		if (tab->toff[i].off == 255) break;
		
		if (tab->toff[i].off != 254) {
			sio_out(tab->toff[i].type);
			sio_out(base & 0xFF);
			sio_out(base >> 8);
		}
		if (++i >= RELOC_SIZE) {
			tab = tab->next;
			i = 0;
		}
	}
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
			
		case 'r':
			return 0x0D;
			
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
	uint16_t a, b, res;
	uint8_t at, bt, ot;
	char op;
	
	// check if expression stack is empty
	if (!(*eindex)) asm_error("expression stack depletion");
	
	// pop off estack
	op = exp_estack[--*eindex];
	
	// attempt to pop out two values from the value stack
	if (*vindex < 2) asm_error("value stack depletion");
	
	// grab values off the stack
	b = exp_vstack[--*vindex].value;
	bt = exp_vstack[*vindex].type;
	a = exp_vstack[--*vindex].value;
	at = exp_vstack[*vindex].type;
	
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
				if (asm_pass == 0) res = 0;
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
	
	// calculate types
	if (!at || !bt) {
		// any operation with an undefined type will also be undefined
		ot = 0;
	} else if (at != 4 && bt != 4) {
		// operations between two non-absolute types are forbidden
		asm_error("incompatable types");
	} else if (at == 4 && bt != 4) {
		// a is absolute, b is not
		// only addition is allowed here, ot becomes bt
		if (op != '+')
			asm_error("invalid type operation");
		ot = bt;
	} else if (at != 4 && bt == 4) {
		// b is absolute, a is not
		// either addition or subtraction is allowed here, ot becomes at
		if (op != '+' && op != '-')
			asm_error("invalid type operation");
		ot = at;
	} else {
		// both are absolute
		ot = 4;
	}
		
	
	// push into stack
	exp_vstack[*vindex].value = res;
	exp_vstack[(*vindex)++].type = ot;
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
void exp_vstack_push(int *vindex, uint8_t type, uint16_t value)
{
	if (*vindex >= EXP_STACK_DEPTH) asm_error("value stack overflow");
	exp_vstack[*vindex].type = type;
	exp_vstack[(*vindex)++].value = value;
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
 * tok = initial token, 0 if none
 * returns status 0 = unresolved, 1 = text, 2 = data, 3 = bss, 4 = absolute, 5+ = external types
 */
uint8_t asm_evaluate(uint16_t *result, char itok)
{
	char tok, op, type, dosz;
	uint16_t num;
	struct symbol *sym;
	int vindex, eindex;
	
	// reset indicies
	vindex = eindex = 0;
	
	while (1) {
		// read token, or use inital token
		if (itok) {
			tok = itok;
			itok = 0;
		} else tok = asm_token_read();
		
		// default is absolute
		type = 4;
		
		if (tok == 'a' || tok == '$') {
			// it is a symbol
			
			// see if we are doing a size or value operation
			dosz = 0;
			if (tok == '$') {
				dosz = 1;
				tok = asm_token_read();
				if (tok != 'a')
					asm_error("unexpected token");
			}
			
			op = 0;
			sym = asm_sym_fetch(sym_table, token_buf);
			if (sym) {
				
				if (dosz) {
					// all sizes are absolute
					num = sym->size;
				} else {
					// get type
					type = sym->type;
					
					// get value
					num = sym->value;
				}
			} else {
				type = 0;
				num = 0;
			}
			
			// parse subtypes for symbols
			while (sio_peek() == '.') {
				
				asm_token_read();
				tok = asm_token_read();
				
				if (tok != 'a')
					asm_error("unexpected token");
				
				// don't both doing another lookup if sym is null
				if (sym)
					sym = asm_sym_fetch(sym, token_buf);
				
				if (sym) {
					if (dosz) {
						num = sym->size;
					} else {
						num += sym->value;
					}
				} else {
					type = 0;
					num = 0;
				}
			}
		} else if (tok == '0') {
			// it is a numeric (maybe)
			op = 0;
		
			if (asm_num(token_buf[0]) && (token_buf[1] == 'f' || token_buf[1] == 'b') && token_buf[2] == 0) {
				// nope, actually a local label
				type = asm_local_fetch(&num, loc_cnt, asm_char_parse(token_buf[0]), token_buf[1] == 'f');
			} else {
				// its a numeric (for realz)
				num = asm_num_parse(token_buf);
			}
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
			exp_vstack_push(&vindex, type, num);
		}
		
		// check for ending conditions
		tok = sio_peek();
		if (tok == ',' || tok == '\n' || tok == ']' || tok == '}' || tok == -1) break;
		if (tok == ')' && !exp_estack_has_lpar(eindex)) break;
			
	}
	
	while (eindex) exp_estack_pop(&eindex, &vindex);
	
	if (vindex != 1) asm_error("value stack overpopulation");
	
	*result = exp_vstack[0].value;
	
	// return type
	return exp_vstack[0].type;
}

/*
 * parses a bracket if there is one to parses, else returns zero
 * nofail is used to emit an error if the bracket MUST be parsed
 *
 * nofail = fail if internal expression fails to parse
 * returns expression results
 */
uint16_t asm_bracket(char nofail)
{
	uint16_t result;
	char res;
	
	// if there is no bracket, just return 0
	if (sio_peek() != '[')
		return 0;
	
	asm_token_read();
	
	// evaluate
	res = asm_evaluate(&result, 0);
	
	asm_expect(']');
	
	if (!res) {
		if (nofail)
			asm_error("undefined expression");
		
		return 0;
	}
	
	if (res != 4)
		asm_error("must be absolute");
	
	return result;
}

/*
 * emits a byte into assembly output
 * no bytes emitted on first pass, only indicies updated
 *
 * b = byte to emit
 */
void asm_emit(uint8_t b)
{
	if (asm_pass) {
		switch (asm_seg) {
			case 1:
				sio_out((char) b);
				break;
				
			case 2:
				sio_tmp((char) b);
				break;
				
			case 3:
				if (b) 
					asm_error("data in bss");
				break;
				
			default:
				break;
		}
	}
	
	asm_address++;
}

/*
 * emits a little endian word to the binary
 *
 * w = word to emit
 */
void asm_emit_word(uint16_t w) {
	asm_emit(w & 0xFF);
	asm_emit(w >> 8);
}

/*
 * emits a string found in the char stream
 */
void asm_emit_string()
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
					asm_emit(decode);
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
				asm_emit(c);
			
			
		} else if (state == 1) {
			// escape character
			decode = asm_escape_char(c);
			
			// simple escape
			if (decode) {
				asm_emit(decode);
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
				asm_emit(decode);
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
 * size = number of bytes to fille
 */
void asm_fill(uint16_t size)
{
	while (size--) asm_emit(0);
}

/*
 * emits up to two bytes, and handels relocation tracking
 *
 * size = number of bytes to emit
 * value = value to emit
 * type = segment to emit into
 */
void asm_emit_addr(uint16_t size, uint16_t value, uint8_t type)
{
	uint16_t rel;

	
	if (!type) {
		// if we are on the second pass, error out
		if (asm_pass)
			asm_error("undefined symbol");
		
		value = 0;
	}
	
	if (!size)
		asm_error("not a type");
	
	if (size == 1) {
		// here we output only a byte
		if (type > 4 && asm_pass)
			asm_error("cannot extern byte");
		
		if (type > 0 && type < 4) {
			// emit a relative address
			rel = (value - asm_address) - 1;
			if (rel < 0x80 || rel > 0xFF7F)
				asm_emit(rel);
			else
				asm_error("relative out of bounds");
		} else {
			asm_emit(value);
		}
	
	} else {
		
		if (((type > 0 && type < 4) || type > 4) && asm_pass) {
			
			// relocate!
			switch (asm_seg) {
				case 1:
					asm_reloc(&textr, asm_address, type);
					break;
					
				case 2:
					asm_reloc(&datar, asm_address - text_top, type);
					break;
					
				default:
					asm_error("invalid segment");
			}
		}
		
		// here we output a word
		asm_emit_word(value);
	}
}

/*
 * helper function to emit an immediate and do type checking
 * only absolute resolutions will be allowed
 */
void asm_emit_imm(uint16_t value, uint8_t type)
{	
	if (type != 4 && asm_pass)
		asm_error("must be absolute");
	
	asm_emit(value);
}

/*
 * helper function to evaluate an expression and emit the results
 * will handle relocation tracking and pass related stuff
 *
 * size = maximum size of space
 */
void asm_emit_expression(uint16_t size, char tok)
{
	uint16_t value;
	uint8_t type;
	
	type = asm_evaluate(&value, tok);
	
	asm_emit_addr(size, value, type);
}

/*
 * recursive function to do type-based definitions
 */
void asm_define_type(struct symbol *type)
{
	struct symbol *sym;
	char tok;
	uint16_t base, size;
	
	if (!type || !type->size)
		asm_error("not a type");
	
	size = type->size;
	base = asm_address;
	
	// get the first field
	asm_expect('{');

	sym = type->parent;
	while (sym) {
		// correct to required location
		if (asm_address > base + sym->value)
			asm_error("field domain overrun");
		asm_fill((base + sym->value) - asm_address);
		
		tok = sio_peek();
		if (tok == '"') {
			// emit the string
			asm_emit_string();
			
		} else if (tok == '{') {
			// emit the type (recursive)
			asm_define_type(sym->parent);
			
		} else {
			// emit the expression
			asm_emit_expression(sym->size, 0);
		}
		

		if (sym->next) {
			asm_expect(',');
		}
			
		sym = sym->next;
	}
	
	// finish corrections
	if (asm_address > base + size)
		asm_error("field domain overrun");
	asm_fill((base + size) - asm_address);
	
	asm_expect('}');
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
	uint16_t size, addr;
	
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
			asm_emit_string();
			
		} else if (tok == '{') {
			// emit the type
			asm_define_type(parent);
			
		} else {
			// emit the expression
			asm_emit_expression(size, 0);
		}
		
		// see how many elements we emitted, and align to size
		while (asm_address > addr) {
			addr += size;
			i++;
		}
		asm_fill(addr - asm_address);
		

		if (sio_peek() != '\n' && sio_peek() != -1) asm_expect(',');
	}
	
	// do count handling
	if (!count)
		return;
	
	if (i > count)
		asm_error("define domain overrun");
	
	asm_fill(size * (count - i));
}

/*
 * defines a new type structure
 */
void asm_type(char *name)
{
	char tok;
	struct symbol *type, *sym;
	uint16_t base, size, count;
	
	asm_expect('{');
	
	if (asm_pass) {
		while (sio_peek() != '}' && sio_peek() != -1)
			asm_token_read();
		
		asm_expect('}');
		return;
	}
	if (asm_sym_fetch(sym_table, name))
		asm_error("type already defined");
	
	type = asm_sym_update(sym_table, name, 4, NULL, 0);
	
	base = 0;
	while (1) {
		// read type
		tok = asm_token_read();
		
		if (tok != 'a')
			asm_error("expected symbol");
		
		sym = asm_type_size(token_buf, &size);
		
		if (!size)
			asm_error("not a type");
		
		count = asm_bracket(1);
		// there can be no zero counts
		if (!count) count = 1;
		
		// read name
		tok = asm_token_read();
		
		if (tok != 'a')
			asm_error("expected symbol");
		
		sym = asm_sym_update(type, token_buf, 4, sym, base);
		sym->size = size;

		base += size * count;
		
		if (sio_peek() == ',')
			asm_expect(',');
		else
			break;
	}
	type->size = base;
	
	asm_expect('}');
	
}

/*
 * parses an operand, extracting the type of operation and/or constant
 *
 * con = pointer to constant return
 * eval = automatically evaluate expressions into con
 *		  it also converts 'c' from a register to a flag is eval is not on
 * returns type of operand
 */
uint8_t asm_arg(uint16_t *con, uint8_t eval) {
	int i;
	char tok;
	uint8_t ret, type;
	
	// check if there is anything next
	if (sio_peek() == '\n' || sio_peek() == -1)
		return 255;
	
	// assume at plain expression at first
	ret = 31;
	
	// read the token
	tok = asm_token_read();
	
	// maybe a register symbol?
	if (tok == 'a') {
		i = 0;
		while (op_table[i].type != 255) {
			if (asm_sequ(token_buf, op_table[i].mnem)) {
				if (!eval && op_table[i].type == 1)
					return 16;
				return op_table[i].type;
			}
			i++;
		}
	} 
	
	// maybe in parathesis?
	if (tok == '(') {
		tok = asm_token_read();
		
		// check for hl
		if (asm_sequ(token_buf, "hl")) {
			asm_expect(')');
			return 6;
		} 
		
		// check for c
		else if (asm_sequ(token_buf, "c")) {
			asm_expect(')');
			return 33;
		}
		
		// check for sp
		else if (asm_sequ(token_buf, "sp")) {
			asm_expect(')');
			return 34;
		}
		
		// check for bc
		else if (asm_sequ(token_buf, "bc")) {
			asm_expect(')');
			return 35;
		}
		
		// check for de
		else if (asm_sequ(token_buf, "de")) {
			asm_expect(')');
			return 36;
		}
		
		
		// check for ix and iy
		else if (asm_sequ(token_buf, "ix")) {
			if (sio_peek() == '+') {
				// its got a constant
				asm_token_read();
				tok = 0;
				ret = 25;
			} else {
				asm_expect(')');
				return 29;
			}
		} else if (asm_sequ(token_buf,"iy")) {
			if (sio_peek() == '+') {
				// its got a constant
				asm_token_read();
				tok = 0;
				ret = 28;
			} else {
				asm_expect(')');
				return 30;
			}
		} 
		
		// evaluate as deferred expression
		else {
			ret = 32;
		}
	}
	
	// ok, its an expression
	if (eval) {
		type = asm_evaluate(con, tok);
		if (type == 0) {
			*con = 0;
			if (asm_pass)
				asm_error("undefined symbol");
		} else if (type != 4)
			asm_error("must be absolute");
		
		// if not 29, needs a trailing ')'
		if (ret != 31)
			asm_expect(')');
	} else {
		// hack to return tok so the caller can run asm_emit_exp
		*con = tok;
	}
	return ret;
}

/*
 * assembles an instructions
 * if/elses that would make yandev blush
 *
 * isr = pointer to instruct
 * returns 0 if successful
 */
char asm_doisr(struct instruct *isr) {
	uint8_t prim, arg, reg, type;
	uint16_t con, value;
	
	// primary select to 0
	prim = 0;
	if (isr->type == BASIC) {
		// basic ops
		asm_emit(isr->opcode);
	} 
	
	else if (isr->type == BASIC_EXT) {
		// basic extended ops
		asm_emit(isr->arg);
		asm_emit(isr->opcode);
	} 
	
	else if (isr->type == ARITH) {
		// arithmetic operations
		arg = asm_arg(&con, 1);
		
		// detect type of operation
		if (isr->arg == CARRY) {
			// carry op
			if (arg == 10) {
				// hl adc/sbc
				prim = 1;
			} else  if (arg != 7)
				return 1;
			
			// grab next arg
			asm_expect(',');
			arg = asm_arg(&con, 1);
		} else if (isr->arg == ADD) {
			// add op
			if (arg == 10) {
				// hl add
				prim = 2;
				
			} else if (arg == 21 || arg == 22) {
				// ix/iy add
				prim = 3;
				reg = arg;
			} else if (arg != 7)
				return 1;
			
			// grab next arg
			asm_expect(',');
			arg = asm_arg(&con, 1);
			
			// no add i*,hl
			if (prim == 3 && arg == 10)
				return 1;
			
			// add i*,i*
			if (prim == 3 && arg == reg)
				arg = 10;
		}
		
		if (prim == 0) {
			if (arg < 8) {
				// basic from a-(hl)
				asm_emit(isr->opcode + arg);
			} else if (arg >= 23 && arg <= 25) {
				// ix class
				asm_emit(0xDD);
				asm_emit(isr->opcode + (arg - 23) + 4);
				if (arg == 25)
					asm_emit(con & 0xFF);
			} else if (arg >= 26 && arg <= 28) {
				// iy class
				asm_emit(0xFD);
				asm_emit(isr->opcode + (arg - 26) + 4);
				if (arg == 28)
					asm_emit(con & 0xFF);
			} else if (arg == 31) {
				// constant
				asm_emit(isr->opcode + 0x46);
				asm_emit(con);
			} else 
				return 1;
		} else if (prim == 1) {
			// 16 bit carry ops bc-sp
			if (arg >= 8 && arg <= 11) {
				asm_emit(0xED);
				asm_emit((0x42 + (isr->opcode == 0x88 ? 8 : 0)) + ((arg-8)<<4));
			} else 
				return 1;
		} else if (prim == 2) {
			// 16 bit add ops bc-sp
			if (arg >= 8 && arg <= 11) {
				asm_emit(0x09 + ((arg-8)<<4));
			} else
				return 1;
		} else if (prim == 3) {
			// correct for hl -> ix,iy
			if (arg == 10)
				arg = reg;
			if (arg == reg)
				arg = 10;
			
			// pick ext block
			if (reg == 21)
				asm_emit(0xDD);
			else
				asm_emit(0xFD);
			
			// 16 bit add ops bc-sp
			if (arg >= 8 && arg <= 11) {
				asm_emit(0x09 + ((arg-8)<<4));
			} else 
				return 1;
		}
	}
	
	else if (isr->type == INCR) {
		// increment / decrement
		arg = asm_arg(&con, 1);
		
		if (arg < 8) {
			// basic from a-(hl)
			asm_emit(isr->opcode + ((arg)<<3));
		} else if (arg < 12) {
			// words bc-sp
			asm_emit(isr->arg + ((arg-8)<<4));
		} else if (arg == 21) {
			// ix
			asm_emit(0xDD);
			asm_emit(isr->arg + 0x20);
		} else if (arg == 22) {
			// iy
			asm_emit(0xFD);
			asm_emit(isr->arg + 0x20);
		} else if (arg >= 23 && arg <= 25) {
			// ixh-(ix+*)
			asm_emit(0xDD);
			asm_emit(isr->opcode + ((arg-19)<<3));
			if (arg == 25)
				asm_emit(con);
		} else if (arg >= 26 && arg <= 28) {
			// iyh-(iy+*)
			asm_emit(0xFD);
			asm_emit(isr->opcode + ((arg-22)<<3));
			if (arg == 28)
				asm_emit(con);
		} else
			return 1;
	}
	
	else if (isr->type == BITSH) {
		// bit / shift
		arg = asm_arg(&con, 1);
		
		// bit instructions have a bit indicator that must be parsed
		reg = 0;
		if (isr->arg) {
			if (arg != 31)
				return 1;
			
			if (con > 7)
				return 1;
			
			reg = con;
			
			// grab next
			asm_expect(',');
			arg = asm_arg(&con, 1);
		}
		
		// check for (ix+*) / (iy+*)
		if (arg == 25 || arg == 28) {
			
			if (arg == 25)
				asm_emit(0xDD);
			else
				asm_emit(0xFD);
			
			asm_emit(0xCB);
			
			// write offset
			asm_emit(con);
			
			arg = 6;
			// its an undefined operation
			if (sio_peek() == ',') {
				asm_expect(',');
				arg = asm_arg(&con, 1);
				
				// short out for (hl)
				if (arg == 6)
					arg = 8;
			}
		} else
			asm_emit(0xCB);			
	
		if (arg > 7)
			return 1;
		
		asm_emit(isr->opcode + arg + (reg<<3));
	}
	
	else if (isr->type == STACK) {
		// stack operation
		arg = asm_arg(&con, 1);
		
		// swap af for sp
		if (arg == 11)
			arg = 12;
		else if (arg == 12)
			arg = 11;
		
		if (arg >= 8 && arg <= 11) {
			// bc-af
			asm_emit(isr->opcode + ((arg-8)<<4));
		} else if (arg == 21) {
			asm_emit(0xDD);
			asm_emit(isr->opcode + 0x20);
		} else if (arg == 22) {
			asm_emit(0xFD);
			asm_emit(isr->opcode + 0x20);
		} else
			return 1;
	}
	
	else if (isr->type == RETFLO) {
		// return
		arg = asm_arg(&con, 0);
		
		if (arg >= 13 && arg <= 20) {
			asm_emit(isr->opcode + ((arg-13)<<3));
		} else if (arg == 255) {
			asm_emit(isr->arg);
		} else
			return 1;
	}
	
	else if (isr->type == JMPFLO) {
		// jump (absolute)
		arg = asm_arg(&con, 0);
		
		if (arg >= 13 && arg <= 20) {
			asm_emit(isr->opcode + ((arg-13)<<3));
			asm_expect(',');
			asm_emit_expression(2, 0);
		} else if (arg == 31) {
			asm_emit(isr->opcode + 1);
			asm_emit_expression(2, con);
		} else if (arg == 6) {
			asm_emit(isr->arg);
		} else if (arg == 29) {
			asm_emit(0xDD);
			asm_emit(isr->arg);
		} else if (arg == 30) {
			asm_emit(0xFD);
			asm_emit(isr->arg);
		} else
			return 1;
	}
	
	else if (isr->type == JRLFLO) {
		// jump (relative)
		arg = asm_arg(&con, 0);
		
		// jr allows for 4 conditional modes
		reg = 0;
		if (isr->arg) {
			if (arg >= 13 && arg <= 16) {
				reg = (arg - 12)<<3;
				asm_expect(',');
				arg = asm_arg(&con, 0);
			} else if (arg != 31)
				return 1;
		}
		
		if (arg != 31)
			return 1;
		
		asm_emit(isr->opcode + reg);
		asm_emit_expression(1, con);
	}
	
	else if (isr->type == CALFLO) {
		// call
		arg = asm_arg(&con, 0);
		
		if (arg >= 13 && arg <= 20) {
			asm_emit(isr->opcode + ((arg-13)<<3));
			asm_expect(',');
			asm_emit_expression(2, 0);
		} else if (arg == 31) {
			asm_emit(isr->arg);
			asm_emit_expression(2, con);
		} else
			return 1;
	}
	
	else if (isr->type == RSTFLO) {
		// rst
		arg = asm_arg(&con, 1);
		
		if (arg != 31 || con & 0x7 || con > 0x38)
			return 1;
		
		
		asm_emit(isr->opcode + con);
	}
	
	else if (isr->type == IOIN) {
		// in
		arg = asm_arg(&con, 1);
		
		// special case for (c) only
		if (arg == 33) {
			asm_emit(0xED);
			asm_emit(isr->arg + 0x30);
			return 0;
		}
		
		// throw out (hl)
		if (arg == 6 || arg > 7)
			return 1;
		
		// grab next argument
		reg = arg;
		asm_expect(',');
		arg = asm_arg(&con, 1);
		
		// decode
		if (reg == 7 && arg == 32) {
			asm_emit(isr->opcode);
			asm_emit(con);
		} else if (arg == 33) {
			asm_emit(0xED);
			asm_emit(isr->arg + (reg<<3));
		} else
			return 1;
	}
	
	else if (isr->type == IOOUT) {
		// out
		arg = asm_arg(&con, 1);
		
		if (arg == 32) {
			// immedate
			reg = con;
			asm_expect(',');
			arg = asm_arg(&con, 1);
			
			// only 'a' is supported
			if (arg != 7)
				return 1;
			
			asm_emit(isr->opcode);
			asm_emit(reg);
		} else if (arg == 33) {
			// (c)
			asm_expect(',');
			arg = asm_arg(&con, 1);
			
			// no (hl), but we can do '0' instead
			if (arg == 6)
				return 1;
			if (arg == 31 && !con)
				arg = 6;
				
			if (arg > 7)
				return 1;
			
			asm_emit(0xED);
			asm_emit(isr->arg + (arg<<3));
		} else
			return 1;
	}
	
	else if (isr->type == EXCH) {
		// exchange
		reg = asm_arg(&con, 1);
		asm_expect(',');
		arg = asm_arg(&con, 1);
		
		// af
		if (reg == 12) {
			if (arg == 12) {
				asm_expect('\'');
				asm_emit(isr->arg);
			} else
				return 1;
		}
		
		// de
		else if (reg == 9) {
			if (arg == 10) {
				asm_emit(isr->opcode + 0x08);
			} else
				return 1;
		}
		
		// (sp)
		else if (reg == 34) {
			switch (arg) {
				case 10:
					break;
					
				case 21:
					asm_emit(0xDD);
					break;
					
				case 22:
					asm_emit(0xFD);
					break;
					
				default:
					return 1;
			}
			
			asm_emit(isr->opcode);
		}
	}
	
	else if (isr->type == INTMODE) {
		// interrupt mode
		arg = asm_arg(&con, 1);
		
		// only 0-2
		if (arg != 31)
			return 1;
		
		asm_emit(0xED);
		switch (con) {
			case 0:
			case 1:
				asm_emit(isr->opcode + (con<<4));
				break;
				
			case 2:
				asm_emit(isr->arg);
				break;
				
			default:
				return 1;
		}
	}
	
	else if (isr->type == LOAD) {
		// load instruction
		// i will never forgive you zilog
		
		// correct for carry flag
		arg = asm_arg(&con, 0);
		if (arg == 16)
			arg = 1;
		
		// special case for deferred constant
		if (arg == 32) {
			type = asm_evaluate(&value, con);
			asm_expect(')');
			asm_expect(',');
			arg = asm_arg(&con, 1);
			
			switch (arg) {
				case 10:
					asm_emit(0x22);
					break;
					
				case 7:
					asm_emit(0x32);
					break;
					
				case 21:
					asm_emit(0xDD);
					asm_emit(0x22);
					break;
					
				case 22:
					asm_emit(0xFD);
					asm_emit(0x22);
					break;
					
				case 8:
				case 9:
				case 11:
					asm_emit(0xED);
					asm_emit(0x43 + ((arg-8)<<4));
					break;
					
				default:
					return 1;
			}
			
			asm_emit_addr(2, value, type);
		}
		
		// standard a-(hl) and ixh-(iy+*)
		else if (arg < 8 || (arg >= 23 && arg <= 28)) {
			// grab any constants if they exist
			if (arg == 25 || arg == 28) {
				type = asm_evaluate(&value, con);
				asm_expect(')');
				prim++;
			}
			asm_expect(',');
			
			// correct for carry flag
			reg = asm_arg(&con, 0);
			if (reg == 16)
				reg = 1;
			
			// i* class dest?
			if (arg >= 23 && arg <= 28) {
				// check for ix or iy, correct iy
				if (arg <= 25) {
					// ix
					asm_emit(0xDD);
				} else {
					// iy
					asm_emit(0xFD);
					
					// iy should now act like ix
					if (reg >= 23 && reg <= 28) {
						if (reg < 26)
							return 1;
						reg = reg - 3;		
					}
					arg = arg - 3;
				}
				
				// check if arg is (ix+*) or not
				if (arg == 25) {
					// no (hl)
					if (reg == 6)
						return 1;
				} else {
					// no h-(hl)
					if (reg >= 4 && reg <= 6)
						return 1;
					
					// downconvert ix*
					if (reg >= 23 && reg <= 25) {
						if (reg == 25)
							return 1;
						reg = reg - 19;
					}
				}
				
				arg = arg - 19;
				
			}
			
			// i* class src?
			else if (reg >= 23 && reg <= 28) {
				// no (hl)
				if (arg == 6)
					return 1;
				
				// check for ix or iy, correct iy
				if (reg <= 25) {
					// ix
					asm_emit(0xDD);
				} else {
					// iy
					asm_emit(0xFD);
					
					// iy should now act like ix
					if (reg >= 23 && reg <= 28) {
						if (reg < 26)
							return 1;
						reg = reg - 3;		
					}
				}
				
				// grab ix/iy offset
				// h/l only allowed for ix+*
				if (reg == 25) {
					type = asm_evaluate(&value, con);
					asm_expect(')');
					prim++;
				} else if (arg == 4 || arg == 5)
					return 1;
				
				reg = reg - 19;
			}
			
			// no (hl),(hl)
			if (arg == 6 && reg == 6)
				return 1;
			
			if (arg < 8 && reg < 8) {
				// reg8->reg8
				asm_emit(0x40 + (arg<<3) + reg);
				if (prim)
					asm_emit_imm(value, type);
			} else if (arg < 8 && reg == 31) {
				// *->reg8
				asm_emit(0x06 + (arg<<3));
				if (prim)
					asm_emit_imm(value, type);
				type = asm_evaluate(&value, con);
				asm_emit_imm(value, type);
			} else if (arg == 7) {
				// special a loads
				switch (reg) {
					case 35:
						asm_emit(0x0A);
						break;
					
					case 36:
						asm_emit(0x1A);
						break;
						
					case 32:
						asm_emit(0x3A);
						asm_emit_expression(2, con);
						asm_expect(')');
						break;
						
					case 37:
						asm_emit(0xED);
						asm_emit(0x57);
						break;
						
					case 38:
						asm_emit(0xED);
						asm_emit(0x5F);
						break;
						
					default:
						return 1;
				}
			} else
				return 1;
			
		
		}
		
		
		// bc-sp, and ix/iy
		else if ((arg >= 8 && arg <= 11) || (arg == 21 || arg == 22)) {
			// correct for ix,iy into hl
			if (arg == 21) {
				asm_emit(0xDD);
				arg = 10;
			} else if (arg == 22) {
				asm_emit(0xFD);
				arg = 10;
			} 
			
			// grab a direct or deferred word
			asm_expect(',');
			reg = asm_arg(&con, 0);
			
			if (reg == 31) {
				// direct
				asm_emit(0x01 + ((arg-8)<<4));
				asm_emit_expression(2, con);
			} else if (reg == 32) {
				// deferred
				if (arg == 10) {
					// do hl
					asm_emit(0x2A);
				} else {
					// bc, de, sp
					asm_emit(0xED);
					asm_emit(0x4B + ((arg-8)<<4));
				}
				asm_emit_expression(2, con);
				asm_expect(')');
			} else if (arg == 11) {
				// sp specials
				switch (reg) {
					case 10:
						break;
						
					case 21:
						asm_emit(0xDD);
						break;
						
					case 22:
						asm_emit(0xFD);
						break;
						
					default:
						return 1;
				}
				asm_emit(0xF9);
			} else
				return 1;
			
		}
		
		// special loads
		else if (arg >= 35 && arg <= 38) {
			asm_expect(',');
			reg = asm_arg(&con, 1);
			if (reg != 7)
				return 1;
			
			switch (arg) {
				case 35:
					asm_emit(0x02);
					break;
					
				case 36:
					asm_emit(0x12);
					break;
					
				case 37:
					asm_emit(0xED);
					asm_emit(0x47);
					break;
					
				case 38:
					asm_emit(0xED);
					asm_emit(0x4F);
					break;
			}
		} else
			return 1;
	}
	
	return 0;
}

/*
 * attempts to assemble an instruction assuming a symbol has just been tokenized
 *
 * in = pointer to string
 * returns 0 if an instruction is not matched, 1 if it is
 */
char asm_instr(char *in)
{
	int i;
	
	// search for and assemble instruction
	i = 0;
	while (isr_table[i].type) {
		if (asm_sequ(in, isr_table[i].mnem)) {
			if (asm_doisr(&isr_table[i]))
				asm_error("invalid operand");;
			return 1;
		}
		
		i++;
	}
	
	return 0;
}

/*
 * changes segments for first pass segment top tracking
 *
 * next = next segment
 */
void asm_change_seg(char next)
{
	switch (asm_seg) {
		case 1:
			text_top = asm_address;
			break;
			
		case 2:
			data_top = asm_address;
			break;
			
		case 3:
			bss_top = asm_address;
			break;
			
		default:
			break;
	}
	
	switch (next) {
		case 1:
			asm_address = text_top;
			break;
			
		case 2:
			asm_address = data_top;
			break;
			
		case 3:
			asm_address = bss_top ;
			break;
			
		default:
			break;
	}
}

/*
 * iterates through and fixes all segments for the second pass
 */
void asm_fix_seg()
{
	struct symbol *sym;
	struct local *loc;
	
	sym = sym_table->parent;
	
	while (sym) {
		
		// printf("fixed %s from %d:%d to ", sym->name, sym->type, sym->value);
		
		// data -> text
		if (sym->type == 2) {
			sym->value += text_top;
		}
		
		// bss -> text
		if (sym->type == 3) {
			sym->value += text_top + data_top;
		}
		
		// printf("%d:%d\n", sym->type, sym->value);
		
		sym = sym->next;
	}
	
	loc = loc_table;
	while (loc) {
		
		// printf("fixed $%d from %d:%d to ", loc->label, loc->type, loc->value);
		
		// data -> text
		if (loc->type == 2) {
			loc->value += text_top;
		}
		
		// bss -> text
		if (loc->type == 3) {
			loc->value += text_top + data_top;
		}
		
		// printf("%d:%d\n", loc->type, loc->value);
		
		loc = loc->next;
	}
}

/*
 * outputs the metadata blocks after assembly is done
 */
void asm_meta()
{
	int i;
	uint8_t lextn;
	struct global *glob;
	
	// output size of reloc records
	reloc_rec++;
	sio_out(reloc_rec & 0xFF);
	sio_out(reloc_rec >> 8);
					
	// output reloc table
	asm_reloc_out(textr.head, 0);
	asm_reloc_out(datar.head, text_top);
	
	// output terminator
	sio_out(0);
	sio_out(0);
	sio_out(0);
	
	// output size of global records
	sio_out(glob_rec & 0xFF);
	sio_out(glob_rec >> 8);
	
	// output all globals
	glob = glob_table;
	lextn = 5;
	while (glob) {
		
		// make sure that we aren't outputting the same external twice
		// hard to do, but may be possible
		if (glob->symbol->type > 4)
			if (glob->symbol->type != lextn++)
				asm_error("multiple external emissions");
		
		
		// size-1 bytes for the name
		i = 0;
		while (i < SYMBOL_NAME_SIZE-1) {
			sio_out(glob->symbol->name[i]);
			i++;
		}
		// 1 for the type
		sio_out(glob->symbol->type);
		// 2 for the value
		sio_out(glob->symbol->value & 0xFF);
		sio_out(glob->symbol->value >> 8);
		
		glob = glob->next;
	}
}

/*
 * perform assembly functions
 */
void asm_assemble(char flagg, char flagv)
{
	char tok, type, next;
	int ifdepth, trdepth;
	uint16_t result, size;
	struct symbol *sym;

	// reset data structures
	asm_reset();

	// start at pass 1
	asm_pass = 0;

	// assembler start at 0;
	asm_address = 0;
	
	// reset the segments too
	asm_seg = 1;
	text_top = data_top = bss_top = 0;
	
	// reset local count
	loc_cnt = 0;
	
	// reset records
	glob_rec = reloc_rec = 0;
	
	// reset if and true depth
	ifdepth = trdepth = 0;
	
	// fill space for a.out header
	asm_fill(16);
	
	// general line input stuff
	while (1) {
		// read the next 
		tok = asm_token_read();
		//if (tok != 'a') printf("reading: %c\n", tok);
		//else printf("reading: %s\n", token_buf);
		
		// at end of assembly, next pass or done
		if (tok == -1) {
			
			if (ifdepth)
				asm_error("unpaired .if");
			
			if (!asm_pass) {
				// first pass -> second pass
				if (flagv)
					printf("first pass done, %d Z80 bytes used (%d:%d:%d:%d)\n", (18 * sym_count) + (6 * loc_count) + (4 * glob_count) + ((2 + RELOC_SIZE*2) * reloc_count), sym_count, loc_count, glob_count, reloc_count);
				asm_pass++;
				loc_cnt = 0;
				
				// fix segment symbols
				asm_change_seg(1);
				asm_fix_seg();
				
				// store bss_top for header emission
				size = text_top + data_top + bss_top;
				
				// reset segment addresses
				bss_top = text_top + data_top;
				data_top = text_size = text_top;
				asm_address = text_top = 0;
				asm_seg = 1;
				
				sio_rewind();
				
				// emit header
				
				// magic number
				asm_emit(0x18);
				asm_emit(0x0E);
				
				// info byte
				asm_emit(0x01);
				
				// text base
				asm_emit(0x00);
				asm_emit(0x00);
				
				// syscall vector
				asm_emit(0xC3);
				asm_emit(0x00);
				asm_emit(0x00);
				
				// text entry
				asm_emit(0x00);
				asm_emit(0x00);
				
				// text top
				asm_emit_word(data_top);
				
				// data top
				asm_emit_word(bss_top);
				
				// bss top
				asm_emit_word(size);
				
				
				continue;
			} else {
				// emit relocation data and symbol stuff
				if (flagv)
					printf("second pass done, %d Z80 bytes used (%d:%d:%d:%d)\n", (18 * sym_count) + (6 * loc_count) + (4 * glob_count)  + ((2 + RELOC_SIZE*2) * reloc_count), sym_count, loc_count, glob_count, reloc_count);
				sio_append();
				
				// output metablock
				asm_meta();
				
				break;
			}
		}
		
		// command read
		if (tok == '.') {
			tok = asm_token_read();
			
			if (tok != 'a')
				asm_error("expected directive");
			
			
			// if directive
			if (asm_sequ(token_buf, "if")) {
				ifdepth++;
				
				// evaluate the expression
				type = asm_evaluate(&result, 0);
				
				if (type != 4)
					asm_error("must be absolute");
				
				if (result)
					trdepth++;
				
				asm_eol();
				continue;
			}
			
			// endif directive
			else if (asm_sequ(token_buf, "endif")) {
				if(!ifdepth)
					asm_error("unpaired .endif");
				
				if (ifdepth == trdepth)
					trdepth--;
				ifdepth--;
				
				asm_eol();
				continue;
			}
			
			// skip if in an untrue if segment
			if (ifdepth > trdepth) {
				asm_skip();
				continue;
			}
		
			
			next = 0;
			if (asm_sequ(token_buf, "text")) {
				next = 1;
			} else if (asm_sequ(token_buf, "data")) {
				next = 2;
			} else if (asm_sequ(token_buf, "bss")) {
				next = 3;
			}
			
			// change segment
			if (next != 0) {
				asm_change_seg(next);
				asm_seg = next;
				asm_eol();
				continue;
			}
			
			// globl directive
			else if (asm_sequ(token_buf, "globl")) {
				// these can be chained with commas
				while (1) {
					tok = asm_token_read();
					if (tok != 'a') 
						asm_error("expected symbol");
					if (asm_pass) {
						sym = asm_sym_fetch(sym_table, token_buf);
						if (!sym)
							asm_error("undefined symbol");
						if (sym->type > 4)
							asm_error("symbol is external");
						asm_glob(sym);
					}
					
					// see if there is another
					if (sio_peek() == ',')
						asm_expect(',');
					else
						break;
				}
				asm_eol();
			}
			
			// extern directive
			else if (asm_sequ(token_buf, "extern")) {
				// these can be chained with commas
				while (1) {
					tok = asm_token_read();
					if (tok != 'a') 
						asm_error("expected symbol");
					if (!asm_pass) {
						
						if (!extn)
							asm_error("out of externals");
		
						// create external symbol, and increment extern counter
						sym = asm_sym_update(sym_table, token_buf, extn++, NULL, 0);
		
						// output extern to the global table
						// this will ensure that it is outputted so the linker can see it
						asm_glob(sym);
						
					}
					
					// see if there is another
					if (sio_peek() == ',')
						asm_expect(',');
					else
						break;
				}
				asm_eol();
			}
			
			// define directive
			else if (asm_sequ(token_buf, "def")) {
				tok = asm_token_read();
				if (tok != 'a') 
					asm_error("expected symbol");
				asm_token_cache(sym_name);
				result = asm_bracket(1);
				asm_define(sym_name, result);
				asm_eol();

			}
			
			// label define directive
			else if (asm_sequ(token_buf, "defl")) {
				tok = asm_token_read();
				if (tok != 'a') 
					asm_error("expected symbol");
				
				asm_token_cache(sym_name);
				result = asm_bracket(1);
				tok = asm_token_read();
				if (tok != 'a') 
					asm_error("expected symbol");
				
				sym = asm_type_size(sym_name, &size);
				if (!size)
					asm_error("not a type");
				
				sym = asm_sym_update(sym_table, token_buf, asm_seg, sym, asm_address);
				sym->size = size;
				asm_define(sym_name, result);
				asm_eol();
			}
			
			// type directive
			else if (asm_sequ(token_buf, "type")) {
				tok = asm_token_read();
				if (tok == 'a') {
					asm_token_cache(sym_name);
					asm_type(sym_name);
					asm_eol();
				} else
					asm_error("expected symbol");
			} else
				asm_error("unexpected token");
			
			continue;
		}
		
		// skip if in an untrue if segment
		if (ifdepth > trdepth && tok != 'n') {
			asm_skip();
		}
		
		// symbol read
		else if (tok == 'a')  {
			
			// try to get the type of the symbol
			if (asm_instr(token_buf)) {
				// it's an instruction
				asm_eol();
			} else if (sio_peek() == '=') {
				// it's a symbol definition
				asm_token_cache(sym_name);
				asm_token_read();
				
				// evaluate the expression
				type = asm_evaluate(&result, 0);
				
				// set the new symbol
				asm_sym_update(sym_table, sym_name, type, NULL, result);
				asm_eol();
			} else if (sio_peek() == ':') {
				// it's a label
				
				// set the new symbol (if it is the first pass)
				if (!asm_pass) {
					asm_sym_update(sym_table, token_buf, asm_seg, NULL, asm_address);
					
					// auto globals?
					if (flagg) {
						sym = asm_sym_fetch(sym_table, token_buf);
						asm_glob (sym);
					}
				}
				
				asm_token_read();
			} else {
				asm_error("unexpected symbol");
			}
		} 
		
		else if (tok == '0') {
			// numeric read
			result = asm_num_parse(token_buf);
			
			if (result > 9)
				asm_error("local too large");
			
			asm_expect(':');
			
			loc_cnt++;
			if (!asm_pass)
				asm_local_add(result, asm_seg, asm_address);
			
		} else if (tok != 'n') {
			asm_error("unexpected token");
		}
	}
}