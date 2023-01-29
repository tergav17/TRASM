/*
 * reloc.c
 *
 * relocation tool for the TRASM toolchain
 */
 
#include "reloc.h"

#define VERSION "1.0"

/* arg zero */
char *argz;

/* output binary stuff */
FILE *aout;

/* flags */
char flagb = 0;
char flagv = 0;
char flags = 0;

/* relocation bases */
uint16_t base; // primary base
uint16_t bbase; // option bss base

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
	// linking failed, remove a.out
	if (aout) {
		fclose(aout);
		remove("rlout.tmp");
	}
	
	exit(1);
}

/*
 * print usage message
 */
void usage()
{
	printf("usage: %s [-sv] [-b base] input_file base\n", argz);
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
 * helper function for number parsing, returns radix type from character
 *
 * r = radix identifier
 * returns radix type, or 0 if not a radix
 */
char cradix(char r)
{
	if (r == 'b' || r == 'B') return 2;
	if (r == 'o' || r == 'O') return 8;
	if (r == 'x' || r == 'X' || r == 'h' || r == 'H') return 16;
	return 0;
}

/*
 * another helper function, this time to take a decimal / hex char and convert it
 *
 * in = char to convert to number
 * returns number, of -1 if failed
 */
int cparse(char in)
{
	if (in >= '0' && in <= '9') return in - '0';
	else if (in >= 'A' && in <= 'F') return (in - 'A') + 10;
	else if (in >= 'a' && in <= 'f') return (in - 'a') + 10;
	return -1;
}

/*
 * attempts to parse a number into an unsigned 16 bit integer
 *
 * in = pointer to string
 * returns actual value of number
 */
uint16_t nparse(char *in)
{
	int num_start, num_end, i;
	uint16_t out;
	char radix; // 0 = ?, 2 = binary, 8 = octal, 10 = decimal, 16 = hex
	
	// default is base 10
	radix = 10;
	
	// first skip through any leading zeros, and set octal if there is some
	for (num_start = 0; in[num_start] == '0'; num_start++)
		radix = 8;
	
	// lets also find the end while we are at it
	for (num_end = 0; in[num_end] != 0; num_end++);
	
	// check and see if there is a radix identifier here
	if ((i = cradix(in[num_start]))) {
		radix = i;
		num_start++;
	} else {
		// lets check at the end too
		if ((i = cradix(in[num_end - 1]))) {
			radix = i;
			num_end--;
		}
	}
	
	// now to parse
	out = 0;
	for (; num_start < num_end; num_start++) {
		i = cparse(in[num_start]);
		
		// error checking
		// this is going to occur in arg parsing, so we use usage() instead
		if (i == -1) usage();
		if (i >= radix) usage();
		
		out = (out * radix) + i;
	}
	
	return out;
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
					
					case 'v': // verbose output
						flagv++;
						break;
						
					case 'b': // alternative base for bss section
						flagb++;
						
						// grab the next argument
						if (++i == argc)
							usage();
						
						bbase = nparse(argv[i++]);
						goto next_arg;
						break;
						
					case 's': // squash output, no symbol table outputted
						flags++;
						break;
						
					default:
						error("invalid option", NULL);
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
					
				case 1:
					base = nparse(argv[i]);
					break;
					
				default:
					usage();
					break;
			}
		}
next_arg:;
	}
	if (p != 2)
		usage();
	
	// now actually do the reloc
	printf("source %s, base = %04x\n", src, base);
	
} 