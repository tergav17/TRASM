#ifndef ASM_H
#define ASM_H

/* includes */
#include <stdint.h>

/* defines */

#define EXP_STACK_DEPTH 16

#define TOKEN_BUF_SIZE 19
#define SYMBOL_NAME_SIZE 9

#define RELOC_SIZE 8

/* structs */

/* special types */
struct tval {
	uint16_t value;
	uint8_t type;
};

struct toff {
	uint8_t off;
	uint8_t type;
};

/* Z80 size = 18 bytes */
struct symbol {
	uint8_t type;
	char name[SYMBOL_NAME_SIZE];
	uint16_t size;
	uint16_t value;
	struct symbol *parent;
	struct symbol *next;
};

/* Z80 size = 6 bytes */
struct local {
	uint8_t type;
	uint8_t label;
	uint16_t value;
	struct local *next;
};

/* Z80 size = RELOC_SIZE*2 + 2 bytes */
struct reloc {
	struct toff toff[RELOC_SIZE];
	struct reloc *next;
};

/* Z80 size = 4 bytes */
struct global {
	struct symbol *symbol;
	struct global *next;
};

/* headers for reloc tables */
struct header {
	uint16_t last;
	uint8_t index;
	struct reloc *head;
	struct reloc *tail;
};


/* interface functions */

void asm_reset();
void asm_assemble(char flagg, char flagv);

#endif