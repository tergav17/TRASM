#ifndef LD_H
#define LD_H

/* includes */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* defines */

#define SYMBOL_NAME_SIZE 9
#define SYMBOL_REC_SIZE ((SYMBOL_NAME_SIZE-1)+3)

#define RELOC_SIZE 8
#define PATCH_SIZE 4

/* structs */

// object header contains general information able how and where data will be linked
struct object {
	char *fname; // file name
	uint8_t index;
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

// patch element
struct patch {
	uint16_t addr[PATCH_SIZE];
	struct patch *next;
};

struct extrn {
	char name[SYMBOL_NAME_SIZE]; // extern reference
	
	uint16_t value; // symbol patch stuff
	uint8_t type;
	struct object *source;
	
	struct extrn *next; // next external
};

#endif