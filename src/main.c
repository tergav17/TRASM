#include <stdio.h>

#include "sio.h"

#define VERSION "1.0"

int main(int argc, char *argv[])
{
	char next;
	// intro message
	printf("TRASM cross assembler v%s\n", VERSION);
	
	// open up the source files
	sio_open(argc, argv);
	
	while (0 < (next = sio_next())) {
		putchar(next);
	}
	
	sio_rewind();
	
	while (0 < (next = sio_next())) {
		putchar(next);
	}
}