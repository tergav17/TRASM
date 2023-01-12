#ifndef ISR_H
#define ISR_H

/* includes */
#include <stdint.h>

/* defines */
#define END 0
#define BASIC 1 // 1 byte instruction, no args
#define BASIC_EXT 2 // 2 byte instruction, no arg 
#define ARITH 3 // arithmetic operation group
#define INCR 4 // increment / decrement group

#define UNARY 0
#define CARRY 1
#define ADD 2

/* structs */
struct instruct {
	uint8_t type;
	char *mnem;
	uint8_t opcode;
	uint8_t arg;
};

struct oprnd {
	uint8_t type;
	char *mnem;
};

/* (simple) operand table */
struct oprnd op_table[] = {
	{ 0, "b" },
	{ 1, "c" },
	{ 2, "d" },
	{ 3, "e" },
	{ 4, "h" },
	{ 5, "l" },
	{ 7, "a" },
	{ 8, "bc" },
	{ 9, "de" },
	{ 10, "hl" },
	{ 11, "sp" },
	{ 12, "af" },
	{ 13, "nz" },
	{ 14, "z" },
	{ 15, "nc" },
	{ 16, "cr" },
	{ 17, "po" },
	{ 18, "pe" },
	{ 19, "p" },
	{ 20, "m" },
	{ 21, "ix" },
	{ 22, "iy" },
	{ 23, "ixh" },
	{ 24, "ixl" },
	{ 26, "iyh" },
	{ 27, "iyl" },
	{ 255, "" }
};

/*
 * b	= 0
 * c 	= 1
 * d	= 2
 * e	= 3
 * h	= 4
 * l	= 5
 * (hl) = 6
 * a	= 7
 * bc	= 8
 * de	= 9
 * hl	= 10
 * sp	= 11
 * af	= 12
 * nz	= 13
 * z	= 14
 * nc	= 15
 * cr	= 16
 * po	= 17
 * pe	= 18
 * p	= 19
 * m	= 20
 * ix	= 21
 * iy	= 22
 * ixh	= 23
 * ixl	= 24
 * (ix+*) = 25
 * iyh	= 26
 * ihl	= 27
 * (iy+*) = 28
 * (ix) = 29
 * (iy) = 30
 * ?	= 31
 * (?)	= 32
 * next is \n = 255
 */

/* instruction table */
struct instruct isr_table[] = {
	// basic instructions
	{ BASIC, "nop", 0x00, 0 },
	{ BASIC, "rlca", 0x07, 0 },
	{ BASIC, "rrca", 0x0F, 0 },
	{ BASIC, "rla", 0x17, 0 },
	{ BASIC, "rra", 0x1F, 0 },
	{ BASIC, "daa", 0x27, 0 },
	{ BASIC, "cpl", 0x2F, 0 },
	{ BASIC, "scf", 0x37, 0 },
	{ BASIC, "ccf", 0x3F, 0 },
	{ BASIC, "halt", 0x76, 0 },
	{ BASIC, "exx", 0xD9, 0 },
	{ BASIC, "di", 0xF3, 0 },
	{ BASIC, "ei", 0xFB, 0 },
	
	// extended basic instrcutions
	{ BASIC_EXT, "neg", 0x44, 0xED },
	{ BASIC_EXT, "retn", 0x44, 0xED },
	{ BASIC_EXT, "reti", 0x4D, 0xED },
	{ BASIC_EXT, "rrd", 0x67, 0xED },
	{ BASIC_EXT, "rld", 0x6F, 0xED },
	{ BASIC_EXT, "ldi", 0xA0, 0xED },
	{ BASIC_EXT, "cpi", 0xA1, 0xED },
	{ BASIC_EXT, "ini", 0xA2, 0xED },
	{ BASIC_EXT, "outi", 0xA3, 0xED },
	{ BASIC_EXT, "ldd", 0xA8, 0xED },
	{ BASIC_EXT, "cpd", 0xA9, 0xED },
	{ BASIC_EXT, "ind", 0xAA, 0xED },
	{ BASIC_EXT, "outd", 0xAB, 0xED },
	{ BASIC_EXT, "ldir", 0xB0, 0xED },
	{ BASIC_EXT, "cpir", 0xB1, 0xED },
	{ BASIC_EXT, "inir", 0xB2, 0xED },
	{ BASIC_EXT, "otir", 0xB3, 0xED },
	{ BASIC_EXT, "lddr", 0xB8, 0xED },
	{ BASIC_EXT, "cpdr", 0xB9, 0xED },
	{ BASIC_EXT, "indr", 0xBA, 0xED },
	{ BASIC_EXT, "otdr", 0xBB, 0xED },
	
	{ ARITH, "add", 0x80, ADD },
	{ ARITH, "adc", 0x88, CARRY },
	{ ARITH, "sub", 0x90, UNARY },
	{ ARITH, "sbc", 0x98, CARRY },
	{ ARITH, "and", 0xA0, UNARY },
	{ ARITH, "xor", 0xA8, UNARY },
	{ ARITH, "or", 0xB0, UNARY },
	{ ARITH, "cp", 0xB8, UNARY },
	
	{ INCR, "inc", 0x04, 0x03 },
	{ INCR, "dec", 0x05, 0x0B },
	
	{ END, "", 0x00, 0x00}
};

#endif