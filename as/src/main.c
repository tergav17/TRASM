#include <stdio.h>
#include <stdlib.h>

#include "sio.h"
#include "asm.h"

#define VERSION "1.0"

/* flags */
char flagv = 0;
char flagg = 0;

/* arg zero */
char *argz;

/*
 * print usage message
 */
void usage()
{
	printf("usage: %s [-vg] source.s ...\n", argz);
	exit(1);
}


int main(int argc, char *argv[])
{
	int i, o;
	
	argz = argv[0];
	
	// flag switch
	for (i = 1; i < argc; i++) {
		if (argv[i][0] == '-') {
			o = 1;
			while (argv[i][o]) {
				switch (argv[i][o++]) {
					case 'g':
						flagg++;
						break;
						
					case 'v':
						flagv++;
						break;
						
					default:
						usage();
				}
			}
		}
	}

	// check to see if there are any actual arguments
	o = 1;
	for (i = 1; i < argc; i++)
		if (argv[i][0] != '-')
			o = 0;
	if (o)
		usage();
	
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