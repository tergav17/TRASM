/*
 * strip.c
 *
 * symbol stripping tool for TRASM toolchain
 */
 
#include "strip.h"

#define VERSION "1.0"

/* file output */
FILE *aout;

/* arg zero */
char *argz;

/* buffers */
uint8_t header[16];
uint8_t tmp[512];


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
	printf("usage: %s object.o\n", argz);
	exit(1);
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
 * strips the symbol table off the object file
 *
 * src = source
 */
void strip(char *src)
{
	FILE *f;
	uint16_t bsize, chunk;
	
	// read in header
	f = xfopen(src, "rb");
	fread(header, 16, 1, f);
	
	// start doing checking
	if (header[0x00] != 0x18 || header[0x01] != 0x0E)
		error("%s not an object file", src);
	
	// grab sizes
	bsize = rlend(&header[0x0C]) - 0x10;
	
	// write header
	fwrite(header, 16, 1, aout);
	
	// write out binary section
	while (bsize) {
		
	}
	
	// close file
	xfclose(f);
}


int main(int argc, char *argv[])
{
	// record arg zero
	argz = argv[0];

	// make sure args make sense
	if (argc != 2)
		usage();
	
	// open output file
	aout = xfopen(TMP_FILE, "wb");
	
	// print size
	strip(argv[1]);
	
	// close and move output file
	xfclose(aout);
	rename(TMP_FILE, argv[1]);
} 