#ifndef ASM_H
#define ASM_H

/* includes */
#include <stdint.h>

/* defines */

#define HEAP_SIZE_KB 48
#define HEAP_SIZE (1024 * HEAP_SIZE_KB)
#define EXP_STACK_DEPTH 16

#define TOKEN_BUF_SIZE 19
#define SYMBOL_NAME_SIZE 9

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

/* Z80 size = 4 bytes */
struct reloc {
	uint16_t address;
	struct reloc *next;
};

/* Z80 size = 6 bytes */
struct extrn {
	struct symbol *symbol;
	struct reloc *reloc;
	struct extrn *next;
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
void asm_assemble();

#endif