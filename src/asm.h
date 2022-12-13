#ifndef ASM_H
#define ASM_H

/* includes */
#include <stdint.h>

/* defines */

#define HEAP_SIZE_KB 48
#define HEAP_SIZE (1024 * HEAP_SIZE_KB)
#define EXP_STACK_DEPTH 16

#define TOKEN_BUF_SIZE 16

/* structs */

struct symbol {
	char type;
	char name[9];
	uint8_t size;
	uint16_t value;
	struct symbol *parent;
	struct symbol *next;
};

struct value_list {
	uint16_t value;
	struct value_list *next;
};

/* interface functions */

void asm_reset();
void asm_pass(int pass);

#endif