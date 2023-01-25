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
	
	struct extrn *head; // external head
	struct extrn *tail;
	
	struct object *next; // next object
	
};

struct archive {
	char *fname;
	struct archive *next;
};

struct extrn {
	char name[SYMBOL_NAME_SIZE]; // extern reference
	uint8_t number; // external number for object
	
	uint16_t value; // symbol patch stuff
	uint8_t type;
	struct object *source;
	
	struct extrn *next; // next external
};

#endif