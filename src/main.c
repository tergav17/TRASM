#include <stdio.h>

#include "sio.h"
#include "asm.h"

#define VERSION "1.0"

int main(int argc, char *argv[])
{
	// intro message
	printf("TRASM cross assembler v%s\n", VERSION);
	
	// open up the source files
	sio_open(argc, argv);
	
	//reset assembler heap and data structures
	asm_reset();
	
	// do first pass
	asm_pass(0);
}