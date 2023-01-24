#ifndef ASM_H
#define ASM_H

/* includes */
#include <stdint.h>

/* defines */

#define EXP_STACK_DEPTH 16

#define TOKEN_BUF_SIZE 19
#define SYMBOL_NAME_SIZE 9

#define RELOC_SIZE 8
#define EXTRN_SIZE 6

/* structs */

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

/* Z80 size = RELOC_SIZE + 2 bytes */
struct reloc {
	uint8_t off[RELOC_SIZE];
	struct reloc *next;
};

/* Z80 size = RELOC_SIZE*2 + 2 bytes */
struct extrn {
	uint8_t toff[EXTRN_SIZE];
	struct treloc *next;
};

/* Z80 size = 4 bytes */
struct global {
	struct symbol *symbol;
	struct global *next;
};


/* special types */
struct tval {
	uint16_t value;
	uint8_t type;
};

struct toff {
	uint8_t off;
	uint8_t type;
};

/* headers for reloc tables */
struct rheader {
	uint16_t last;
	uint8_t index;
	struct reloc *head;
	struct reloc *tail;
};

struct eheader {
	uint16_t last;
	uint8_t index;
	struct extrn *head;
	struct extrn *tail;
};

/* interface functions */

void asm_reset();
void asm_assemble(char flagg, char flagv);

#endif