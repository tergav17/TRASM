#ifndef LD_H
#define LD_H

/* includes */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

/* defines */

#define SYMBOL_NAME_SIZE 9

/* structs */

struct object {
	char *fname; // file name
	
	uint16_t obj_base; // intial object base
	
	uint16_t text_size; // segment sizes
	uint16_t data_size;
	uint16_t bss_size;
	
	uint16_t text_base; // segment bases in final object
	uint16_t data_base;
	uint16_t bss_base;
	
	struct object *next; // next object
	
};

struct symbol {
	uint8_t type;
	char name[SYMBOL_NAME_SIZE];
	uint16_t value;
	struct symbol *next;
};

#endif