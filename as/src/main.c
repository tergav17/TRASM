#include <stdio.h>

#include "sio.h"
#include "asm.h"

#define VERSION "1.0"

char flagv = 0;
char flagg = 0;

int main(int argc, char *argv[])
{
	int i;
	
	// flag switch
	for (i = 0; i < argc; i++) {
		if (argv[i][0] == '-') {
			switch (argv[i][1]) {
				case 0:
					flagg++;
					break;
					
				case 'v':
					flagv++;
					break;
					
				default:
					break;
			}
		}
	}
	
	// intro message
	if (flagv)
		printf("TRASM cross assembler v%s\n", VERSION);
	
	// open up the source files
	sio_open(argc, argv);
	
	// do the assembly
	asm_assemble(flagg, flagv);
	
	// all done
	sio_close();
} 