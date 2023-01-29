#ifndef RELOC_H
#define RELOC_H

/* includes */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

/* defines */

#define SYMBOL_NAME_SIZE 9
#define SYMBOL_REC_SIZE ((SYMBOL_NAME_SIZE-1)+3)

#define RELOC_REC_SIZE 3

/* structs */

// typed value
struct tval {
	uint8_t type;
	uint16_t value;
};

#endif