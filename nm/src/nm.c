/*
 * nm.c
 *
 * name list dumper for TRASM toolchain
 */
 
#include "nm.h"

#define VERSION "1.0"

/* arg zero */
char *argz;

/* flags */
char flagp;
char flagr;
char flagg;
char flagv;
char flagh;

/* buffers */
uint8_t header[16];
uint8_t tmp[SYMBOL_REC_SIZE];

/* tables */
struct symbol *sym_table;

/*
 * prints error message, and exits
 *
 * msg = error message
 */
void error(char *msg, char *issue)
{
	printf("error: ");
	printf(msg, issue);
	printf("\n");
	
	exit(1);
}

/*
 * print usage message
 */
void usage()
{
	printf("usage: %s [-prgvh] object.o\n", argz);
	exit(1);
}

/* 
 * alloc memory and check for success
 *
 * s = size in bytes
 */
void *xalloc(size_t s)
{
	void *o;
	
	if (!(o = malloc(s))) {
		error("cannot alloc", NULL);
	}
	return o;
}

/*
 * close file and check for success
 *
 * f = pointer to file
 */
void xfclose(FILE *f)
{
	if (fclose(f))
		error("cannot close", NULL);
}

/*
 * seek and check for success
 *
 * f = pointer to file
 * off = offset
 * w = whence for seek
 */
void xfseek(FILE *f, off_t off, int w)
{
	if (fseek(f, off, w) < 0)
		error("cannot seek", NULL);
}

/*
 * open file and check for success
 *
 * fname = path to file
 * mode = mode to open
 * returns pointer to file
 */
FILE *xfopen(char *fname, char *mode)
{
	FILE *f;
	
	if (!(f = fopen(fname, mode))) {
		error("cannot open %s", fname);
	}
	return f;
}

/*
 * read little endian, read 2 bytes in little endian format
 *
 * b = pointer to first byte
 * return value of word
 */
uint16_t rlend(uint8_t *b)
{
	return b[0] + (b[1] << 8);
}

/*
 * skips over a relocation or symbol seg
 */
void skipsg(FILE *f, uint8_t size)
{
	uint8_t b[2];
	
	fread(b, 2, 1, f);
	xfseek(f, rlend(b) * size, SEEK_CUR);
}

/*
 * dumps out the symbol table of the object file
 *
 * src = source
 */
void dump (char *src)
{
	FILE *f;
	
	// read in header
	f = xfopen(src, "rb");
	fread(header, 16, 1, f);
	
	// start doing checking
	if (header[0x00] != 0x18 || header[0x01] != 0x0E)
		error("%s not an object file", src);
	
	// write header if desired
	if (!flagh)
		printf("object base: %04x entry: %04x size %04x\n", rlend(&header[0x03]), rlend(&header[0x08]), rlend(&header[0x0E]));
	
	
	// close file and finish
	xfclose(f);
}


int main(int argc, char *argv[])
{
	int i, o, p;
	char *src;
	
	// argument processing
	argz = argv[0];
	p = 0;
	for (i = 1; i < argc; i++) {
		if (argv[i][0] == '-') {
			// flag switch
			o = 1;
			while (argv[i][o]) {
				switch (argv[i][o]) {
					
					case 'p': // do not sort
						flagv++;
						break;
						
					case 'r': // sort in reverse order
						flagr++;
						break;
						
					case 'g': // show only external symbols
						flagg++;
						break;
						
					case 'v': // sort by value
						flagv++;
						break;
						
					case 'h': // supress header output
						flagh++;
						break;
						
					default:
						usage();
						break;
				}
				o++;
			}
		} else {
			// argument switch
			switch (p++) {
				case 0:
					src = argv[i];
					break;
					
				default:
					usage();
					break;
			}
		}
	}
	if (p != 1)
		usage();

	// dump out symbols
	dump(src);
} 