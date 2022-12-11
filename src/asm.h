#ifndef ASM_H
#define ASM_H

/* includes */
#include <stdint.h>

/* defines */

#define HEAP_SIZE_KB 16
#define HEAP_SIZE (1024 * HEAP_SIZE_KB)
#define EXP_STACK_DEPTH 16

/* structs */

struct symbol {
	char type,
	char name[9],
	uint16_t value,
}

/* interface functions */

void asm_pass(int pass);

#endif