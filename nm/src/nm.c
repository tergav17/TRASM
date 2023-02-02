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
 * compares two symbols when sorting
 */
char symcmp(struct symbol *a, struct symbol *b)
{
	char out, *ca, *cb;
	
	ca = a->name;
	cb = b->name;
	
	// compare values or names
	if (flagv) {
		out = a->value < b->value;
	} else {
		while (*ca == *cb && *ca) {
			ca++;
			cb++;
		}
		
		out = *ca < *cb;
	}
	
	// reverse if flagr
	if (flagr)
		out = !out;
	
	// force low if flagp
	if (flagp)
		out = 0;
	
	return out;
}

/*
 * adds a symbol record to the table, sorting as needed
 *
 * 
 */
void sadd(uint8_t *rec)
{
	int i;
	char type;
	struct symbol *new, *sym, *prev;
	
	type = rec[SYMBOL_REC_SIZE-3];
	
	// skip if showing externals only
	if (flagg && type < 5)
		return;
	
	// generate new symbol
	new = (struct symbol *) xalloc(sizeof(struct symbol));
	
	new->type = type;
	new->value = rlend(&rec[SYMBOL_REC_SIZE-2]);
	
	for (i = 0; i < SYMBOL_NAME_SIZE-1; i++) {
		new->name[i] = (char) rec[i];
	}
	new->name[i] = 0;
	new->next = NULL;
	
	// if the table is empty, set as table
	if (!sym_table) {
		sym_table = new;
		return;
	}
	
	// see if this should be come the new table;
	if (symcmp(new, sym_table)) {
		new->next = sym_table;
		sym_table = new;
		return;
	}
	
	// otherwise insert onto table
	for (sym = sym_table; sym; sym = sym->next) {
		if (symcmp(new, sym)) {
			new->next = sym;
			prev->next = new;
			return;
		}
		if (!sym->next) {
			sym->next = new;
			return;
		}
		prev = sym;
	}
	
}

/*
 * dumps out the symbol table of the object file
 *
 * src = source
 */
void dump(char *src)
{
	FILE *f;
	uint16_t nsym;
	struct symbol *sym;
	char ename;
	
	// read in header
	f = xfopen(src, "rb");
	fread(header, 16, 1, f);
	
	// start doing checking
	if (header[0x00] != 0x18 || header[0x01] != 0x0E)
		error("%s not an object file", src);
	
	// write header if desired
	if (!flagh)
		printf("object base: %04x entry: %04x size %04x\n", rlend(&header[0x03]), rlend(&header[0x08]), rlend(&header[0x0E]));
	
	// skip the data segment
	xfseek(f, rlend(&header[0x0C]) - 16, SEEK_CUR);
	
	// skip the relocation segment
	skipsg(f, RELOC_REC_SIZE);
	
	// get number of symbols
	fread(tmp, 2, 1, f);
	nsym = rlend(tmp);
	
	// dump them all out
	while (nsym--) {
		fread(tmp, SYMBOL_REC_SIZE, 1, f);
		sadd(tmp);
	}
	
	// print out symbols
	for (sym = sym_table; sym; sym = sym->next) {
		// value
		printf("%04x ", sym->value);
		
		ename = 'e';
		// type
		switch (sym->type) {
			case 0:
				ename = 'u';
				break;
				
			case 1:
				ename = 't';
				break;
				
			case 2:
				ename = 'd';
				break;
				
			case 3:
				ename = 'b';
				break;
				
			case 4:
				ename = 'a';
				break;
				
			default:
				break;
		}
		
		// print type and name
		printf("%c %s\n", ename, sym->name);
		
	}
	
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