#ifndef ASM_H
#define ASM_H

/* includes */
#include <stdint.h>

/* defines */

#define EXP_STACK_DEPTH 16

#define TOKEN_BUF_SIZE 19
#define SYMBOL_NAME_SIZE 9

#define RELOC_SIZE 8
#define PATCH_SIZE 4

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
	uint8_t addr[RELOC_SIZE];
	struct reloc *next;
};

/* Z80 size = 8 bytes */
struct extrn {
	struct symbol *symbol;
	struct patch *textp;
	struct patch *datap;
	struct extrn *next;
};

/* Z80 size = PATCH_SIZE*2 + 2 bytes */
struct patch {
	uint16_t addr[PATCH_SIZE];
	struct patch *next;
};

/* Z80 size = 4 bytes */
struct global {
	struct symbol *symbol;
	struct global *next;
};

struct tval {
	uint16_t value;
	uint8_t type;
};

/* interface functions */

void asm_reset();
void asm_assemble(char flagg, char flagv);

#endif