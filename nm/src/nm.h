#ifndef NM_H
#define NM_H

/* includes */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

/* defines */

#define SYMBOL_NAME_SIZE 9
#define SYMBOL_REC_SIZE ((SYMBOL_NAME_SIZE-1)+3)

#define RELOC_REC_SIZE 3

/* structs */

struct symbol {
	char name[SYMBOL_NAME_SIZE];
	uint16_t value;
	uint8_t type;
	
	struct symbol *next;
};

#endif