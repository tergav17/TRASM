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
char flagh = 0;

/* relocation bases */
uint16_t tbase; // text base
uint16_t bbase; // optional bss base

/* stream stuff */
FILE *relf;
uint16_t nreloc;

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
	// reloc failed, remove a.out
	if (aout) {
		fclose(aout);
		remove(TMP_FILE);
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
 * write little endian, write 2 bytes of little endian format
 *
 * value = value to write
 * b = byte array
 */
void wlend(uint8_t *b, uint16_t value)
{
	*b = value & 0xFF;
	*(++b) = value >> 8;
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
 * closes up the currectly open stream
 */
void sclose()
{
	xfclose(relf);
}

/*
 * opens a streams to read relocation data
 *
 * obj = object to open
 */
void sopen(char *fname)
{
	// open objects
	relf = xfopen(fname, "rb");
	
	// first one jumps to relocation table
	fread(header, 16, 1, relf);
	xfseek(relf, rlend(&header[0x0C]) - 16, SEEK_CUR);
	
	// now we read in the number of records for each
	fread(tmp, 2, 1, relf);
	nreloc = rlend(tmp);
}

/*
 * reads the next value in the stream into a tval
 * also skip externals
 *
 * out = pointer to output tval
 */
void snext(struct tval *out)
{
	do {
		if (nreloc) {
			nreloc--;
			fread(tmp, 3, 1, relf);
			out->type = tmp[0];
			out->value = rlend(tmp + 1);
		} else {
			out->value = 0;
			out->type = 0;
		}
	} while (out->type > 4);
}

/*
 * do relocations
 */
void reloc(char *fname)
{
	FILE *bin;
	uint16_t value, bsize, last, chunk;
	struct tval next;
	
	// open and read header
	bin = xfopen(fname, "rb");
	fread(header, 16, 1, bin);
	
	// start doing checking
	if (header[0x00] != 0x18 || header[0x01] != 0x0E)
		error("%s not an object file", fname);
	
	if (!(header[0x02] & 0b01))
		error("%s not relocatable", fname);
	
	// account for text base
	tbase -= rlend(&header[0x03]);
	
	// acount for bss base
	bbase -= rlend(&header[0x03]) + rlend(&header[0x0C]);
	
	// set new text base
	wlend(&header[0x03], tbase);
	
	// if we are relocating the bss, we will not be able to further link this object
	if (flagb) {
		header[0x02] &= 0b11111110;
	}
	
	// record how many bytes of binary need to be written
	bsize = rlend(&header[0x0C]) - 16;
	
	// write header back to the output
	fwrite(header, 16, 1, aout);
	
	// open stream and start relocation
	sopen(fname);
	
	last = 0x10;
	snext(&next);
	while (last < bsize) {
		if (next.value) {
			chunk = next.value - last; 
		} else {
			chunk = bsize - last;
		}
		
		if (chunk > 512)
			chunk = 512;
		
		fread(tmp, chunk, 1, bin);
		fwrite(tmp, chunk, 1, aout);
		last += chunk;
		
		if (last == next.value && last < bsize) {
			if (bsize - last < 2)
				error("cannot relocate byte", NULL);
			
			// read 2 bytes
			fread(tmp, 2, 1, bin);
			value = rlend(tmp);
			
			// adjust segment
			switch (next.type) {
				case 1:
				case 2:
					value += tbase;
					break;
					
				case 3:
					if (flagb)
						value += bbase;
					else
						value += tbase;
					break;
					
				default:
					error("undefined segment", NULL);
			}
			
			// write the binary
			wlend(tmp, value);
			fwrite(tmp, 2, 1, aout);
			last += 2;
			
			// grab next
			snext(&next);
		}
	}
	
	sclose();
	
	// if we are doing a headless output, don't output relocation or symbols
	if (flagh)
		return;
	
	// now it is time to move over the relocations
	// they stay the same, so it is simply a matter of copying
	fread(tmp, 2, 1, bin);
	fwrite(tmp, 2, 1, aout);
	
	bsize = rlend(tmp);
	last = 0;
	while (last < bsize) {
		chunk = last - bsize;
		
		if (chunk > 512 / RELOC_REC_SIZE)
			chunk = 512 / RELOC_REC_SIZE;
		
		fread(tmp, chunk * RELOC_REC_SIZE, 1, bin);
		fwrite(tmp, chunk * RELOC_REC_SIZE, 1, aout);
		
		last += chunk;
	}
	
	// last is the symbol table
	// symbols will need to be corrected for their new location

	fread(tmp, 2, 1, bin);
	
	// if flag s, just don't write a symbol table
	if (flags) {
		tmp[0] = tmp[1] = 0;
		fwrite(tmp, 2, 1, aout);
		return;
	} else  {
		fwrite(tmp, 2, 1, aout);
	}
	
	bsize = rlend(tmp);
	last = 0;
	while (last < bsize) {
		chunk = last - bsize;
		
		if (chunk > 512 / SYMBOL_REC_SIZE)
			chunk = 512 / SYMBOL_REC_SIZE;
		
		fread(tmp, chunk * SYMBOL_REC_SIZE, 1, bin);
		
		// read value and adjust segment
		value = rlend(&tmp[SYMBOL_REC_SIZE-2]);
		switch (tmp[SYMBOL_REC_SIZE-2]) {
			case 1:
			case 2:
				value += tbase;
				break;
				
			case 3:
				if (flagb)
					value += bbase;
				else
					value += tbase;
				break;
				
			default:
				break;
		}
		wlend(&tmp[SYMBOL_REC_SIZE-2], value);
		
		// write back symbol
		fwrite(tmp, chunk * SYMBOL_REC_SIZE, 1, aout);
		
		last += chunk;
	}
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
						
					case 'h': // headerless output, text segment is shifted down
						
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
					
				case 1:
					tbase = nparse(argv[i]);
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
	
	// intro message
	if (flagv)
		printf("TRASM relocation tool v%s\n", VERSION);
	
	// open output file
	aout = xfopen(TMP_FILE, "wb");
	
	// do relocations
	reloc(src);
	
	// close and move output file
	xfclose(aout);
	rename(TMP_FILE, "a.out");
	
} 