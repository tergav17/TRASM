#ifndef LD_H
#define LD_H

/* includes */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

/* defines */

#define SYMBOL_NAME_SIZE 9
#define SYMBOL_REC_SIZE (SYMBOL_NAME_SIZE-1)+3

#define RELOC_SIZE 8

/* structs */

// object header contains general information able how and where data will be linked
struct object {
	char *fname; // file name
	size_t offset; // offset of record
	
	uint16_t org; // object address space origin
	
	uint16_t text_size; // segment sizes
	uint16_t data_size;
	uint16_t bss_size;
	
	uint16_t text_base; // segment bases in final object
	uint16_t data_base;
	uint16_t bss_base;
	
	struct object *next; // next object
	
};

struct archive {
	char *fname;
	struct archive *next;
};

// relocation element
struct reloc {
	uint8_t addr[RELOC_SIZE];
	struct reloc *next;
};

struct extrn {
	char name[SYMBOL_NAME_SIZE]; // extern reference
	
	uint16_t value; // symbol resolution stuff
	uint8_t type;
	struct object *source;
	
	struct reloc *reloc; // addresses that need relocating
	
	struct extrn *next; // next external
};

#endif