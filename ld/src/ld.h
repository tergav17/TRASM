#ifndef LD_H
#define LD_H

/* includes */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

/* defines */

#define SYMBOL_NAME_SIZE 8
#define SYMBOL_REC_SIZE SYMBOL_NAME_SIZE+3

/* structs */

// object header contains general information able how and where data will be linked
struct object {
	char *fname; // file name
	
	uint16_t org; // object address space origin
	
	uint16_t text_size; // segment sizes
	uint16_t data_size;
	uint16_t bss_size;
	
	uint16_t text_base; // segment bases in final object
	uint16_t data_base;
	uint16_t bss_base;
	
	struct object *next; // next object
	
};

// corrected symbol that will be used for resolving external symbols
struct symbol {
	uint8_t type;
	char name[SYMBOL_NAME_SIZE+1];
	uint16_t value;
	struct symbol *next;
};

// relocation data for a segment
struct reloc {
	uint16_t offset;
	uint16_t skip;
	uint16_t index;
	uint8_t *reloc;
	
	struct reloc *next;
};

#endif