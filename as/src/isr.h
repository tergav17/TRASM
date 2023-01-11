#ifndef ISR_H
#define ISR_H

/* includes */
#include <stdint.h>

/* defines */
#define END 0
#define BASIC 1 // 1 byte instruction, no args
#define BASIC_EXT 2 // 2 byte instruction, no arg 

/* structs */
struct instruct {
	uint8_t type;
	char *mnem;
	uint8_t opcode;
	uint8_t arg;
};

/* instruction table */
struct instruct isr_table[] = {
	// basic instructions
	{ BASIC, "nop", 0x00, 0},
	{ BASIC, "rlca", 0x07, 0},
	{ BASIC, "rrca", 0x0F, 0},
	{ BASIC, "rla", 0x17, 0},
	{ BASIC, "rra", 0x1F, 0},
	{ BASIC, "daa", 0x27, 0},
	{ BASIC, "cpl", 0x2F, 0},
	{ BASIC, "scf", 0x37, 0},
	{ BASIC, "ccf", 0x3F, 0},
	{ BASIC, "halt", 0x76, 0},
	{ BASIC, "exx", 0xD9, 0},
	{ BASIC, "di", 0xF3, 0},
	{ BASIC, "ei", 0xFB, 0},
	
	// extended basic instrcutions
	{ BASIC_EXT, "neg", 0x44, 0xED},
	{ BASIC_EXT, "retn", 0x44, 0xED},
	{ BASIC_EXT, "reti", 0x4D, 0xED},
	{ BASIC_EXT, "rrd", 0x67, 0xED},
	{ BASIC_EXT, "rld", 0x6F, 0xED},
	{ BASIC_EXT, "ldi", 0xA0, 0xED},
	{ BASIC_EXT, "cpi", 0xA1, 0xED},
	{ BASIC_EXT, "ini", 0xA2, 0xED},
	{ BASIC_EXT, "outi", 0xA3, 0xED},
	{ BASIC_EXT, "ldd", 0xA8, 0xED},
	{ BASIC_EXT, "cpd", 0xA9, 0xED},
	{ BASIC_EXT, "ind", 0xAA, 0xED},
	{ BASIC_EXT, "outd", 0xAB, 0xED},
	{ BASIC_EXT, "ldir", 0xB0, 0xED},
	{ BASIC_EXT, "cpir", 0xB1, 0xED},
	{ BASIC_EXT, "inir", 0xB2, 0xED},
	{ BASIC_EXT, "otir", 0xB3, 0xED},
	{ BASIC_EXT, "lddr", 0xB8, 0xED},
	{ BASIC_EXT, "cpdr", 0xB9, 0xED},
	{ BASIC_EXT, "indr", 0xBA, 0xED},
	{ BASIC_EXT, "otdr", 0xBB, 0xED},
	
	{ END, "", 0x00, 0x00}
};

#endif