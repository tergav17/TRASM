/*
 * ld.c
 *
 * link editor for the TRASM toolchain
 */
 
#include "ld.h"

#define VERSION "1.0"

/* buffers */
uint8_t header[16];
uint8_t tmp[SYMBOL_NAME_SIZE];

/* flags */
char flagv = 0;
char flagr = 0;
char flags = 0;

/* tables */
struct object *obj_table;
struct object *obj_tail;

struct symbol *sym_table;

/* output binary stuff */
FILE *aout;

/* protoville */
void error(char *msg, char *issue);

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
		xfclose(aout);
		remove("ldout.tmp");
	}
	exit(1);
}

/*
 * checks if strings are equal
 *
 * a = pointer to string a
 * b = pointer to string b
 * returns 1 if equal
 */
char sequ(char *a, char *b) {
	while (*a) {
		if (*a != *b)
			return 0;
		a++;
		b++;
	}
	if (*a != *b)
		return 0;
	return 1;
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
 * computes a new address based on what segment it is in
 */
uint16_t cmseg(uint16_t addr, struct object *obj)
{
	// relocate to address 0
	addr -= obj->org + 16;
	
	// text, data, or bss?
	if (addr >= obj->text_size + obj->data_size) {
		addr -= obj->text_size + obj->data_size;
		return addr + obj->bss_base;
	} else if (addr >= obj->text_size) {
		addr -= obj->text_size;
		return addr + obj->data_base;
	} else {
		return addr + obj->text_base;
	}
}

/*
 * checks in an object file and adds it to the table
 *
 * fname = path to object file
 */
void chkobj(char *fname)
{
	FILE *f;
	struct object *obj, *curr;
	
	// alloc object
	obj = (struct object *) xalloc(sizeof(struct object));
	obj->next = NULL;
	obj->fname = fname;
	
	// read the header in
	f = xfopen(fname, "rb");
	fread(header, 16, 1, f);
	
	// start doing checking
	if (header[0x00] != 0x18 || header[0x01] != 0x0E)
		error("%s not an object file", fname);
	
	if (!(header[0x02] & 0x01))
		error("%s not linkable", fname);
	
	// read object text base
	obj->org = rlend(&header[0x03]);
	
	// read sizes
	obj->text_size = rlend(&header[0x0A]);
	obj->data_size = rlend(&header[0x0C]) - obj->text_size;
	obj->bss_size = rlend(&header[0x0E]) - obj->text_size - obj->data_size;
	
	// reduce text by 16 due to the removal of header
	obj->text_size -= 16;
	
	// add it to the table
	if (obj_table) {
		curr = obj_table;
		while (curr->next)
			curr = curr->next;
		curr->next = obj;
	} else {
		obj_table = obj;
	}
	obj_tail = obj;
	
	xfclose(f);
}

/*
 * dumps out the symbol table of an object
 * segment addresses are corrected to their final location
 *
 * obj = object to dump from
 */
void sdump(struct object *obj)
{
	FILE *f;
	struct symbol *sym, *curr;
	uint16_t value;
	int nsym;
	
	// read the header in
	f = xfopen(obj->fname, "rb");
	fread(header, 16, 1, f);
	
	// navigate to start of symbol table
	xfseek(f, rlend(&header[0x0C]), SEEK_SET);
	fread(tmp, 2, 1, f);
	xfseek(f, rlend(tmp), SEEK_CUR);
	fread(tmp, 2, 1, f);
	nsym = rlend(tmp);
	nsym /= SYMBOL_REC_SIZE;
	
	// read in all symbols
	while (nsym--) {
		sym = (struct symbol *) xalloc(sizeof(struct symbol));
		sym->next = NULL;
		
		// read in name
		fread(sym->name, 8, 1, f);
		sym->name[SYMBOL_NAME_SIZE] = 0;
		
		// read in type and value
		fread(tmp, 3, 1, f);
		sym->type = tmp[0];
		value = rlend(tmp+1);
		
		// fix segments
		if (sym->type != 4) {
			// relocate to address 0
			value -= obj->org + 16;
			
			switch (sym->type) {
				
				case 0:
					error("symbol %s is undefined", sym->name);
				
				case 1:
					value = value + obj->text_base;
					break;
					
				case 2:
					value -= obj->text_size;
					value +=  obj->data_base;
					break;
					
				case 3:
					value -= obj->text_size + obj->data_size;
					value += obj->bss_base;
					break;
					
				default:
					error("symbol %s is external", sym->name);
			}
		}
		sym->value = value;
		
		// add it to table
		if (sym_table) {
			curr = sym_table;
			while (1) {
				// error out if symbol already exists
				if (sequ(curr->name, sym->name))
					error("symbol %s already defined", sym->name);
				
				if (curr->next) {
					curr = curr->next;
				} else  {
					curr->next = sym;
					break;
				}
			}
		} else {
			sym_table = sym;
		}
	}
	
	xfclose(f);
}

/*
 * compute the final bases for each segment in all objects
 */
void cmbase()
{
	struct object *curr;
	uint16_t addr;
	
	aout = NULL;
	
	// addr starts at 16, right after the header
	addr = 16;
	
	// compute text bases
	for (curr = obj_table; curr; curr = curr->next) {
		curr->text_base = addr;
		addr += curr->text_size;
	}
	
	// compute data bases
	for (curr = obj_table; curr; curr = curr->next) {
		curr->data_base = addr;
		addr += curr->data_size;
	}
	
	// compute bss bases
	for (curr = obj_table; curr; curr = curr->next) {
		curr->bss_base = addr;
		addr += curr->bss_size;
	}
}

/*
 * emits the head of the output object
 */
void emhead()
{
	// magic number
	header[0x00] = 0x18;
	header[0x01] = 0x0E;
	
	// info byte
	if (flagr) {
		header[0x02] = 0b01;
	} else {
		header[0x02] = 0b11;
	}
	
	// text origin always at 0
	wlend(&header[0x03], 0x0000);
	
	// syscall jump vector unpatched
	header[0x05] = 0xC3;
	header[0x06] = 0x00;
	header[0x07] = 0x00;
	
	// text entry is 0
	wlend(&header[0x08], 0x0000);
	
	// text top
	wlend(&header[0x0A], obj_tail->text_base + obj_tail->text_size);
	
	// data top
	wlend(&header[0x0C], obj_tail->data_base + obj_tail->data_size);
	
	// bss top
	wlend(&header[0x0E], obj_tail->bss_base + obj_tail->bss_size);

	// write it out
	fwrite(header, 16, 1, aout);
}

int main(int argc, char *argv[])
{
	int i, o;
	struct object *curr;
	struct symbol *scurr;
	
	// flag switch
	for (i = 1; i < argc; i++) {
		if (argv[i][0] == '-') {
			o = 1;
			while (argv[i][o]) {
				switch (argv[i][o]) {
					
					case 'v': // verbose output
						flagv++;
						break;
						
					case 'r': // output unresolved externals for more linking
						flagr++;
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
		}
	}
	
	// check for invalid configurations
	if (flags && flagr)
		error("invalid configuration", NULL);
	
	
	// intro message
	if (flagv)
		printf("TRASM link editor v%s\n", VERSION);
	
	// check in all object files
	for (i = 1; i < argc; i++) {
		if (argv[i][0] != '-')
			chkobj(argv[i]);
	}
	
	// calculate bases
	cmbase();
	
	// print out objects
	if (flagv) {
		printf("object file base/size:\n");
		for (curr = obj_table; curr; curr = curr->next) {
			printf("	text: %04x:%04x, data: %04x:%04x, bss: %04x:%04x <- %s\n", curr->text_base, curr->text_size, curr->data_base, curr->data_size, curr->bss_base, curr->bss_size, curr->fname);
		}
	}
	
	// dump symbols
	for (curr = obj_table; curr; curr = curr->next) {
		sdump(curr);
	}
	
	// print out symbols
	if (flagv) {
		printf("symbol type/value:\n");
		for (scurr = sym_table; scurr; scurr = scurr->next) {
			printf("	%-8s: %04x ", scurr->name, scurr->value);
			switch(scurr->type) {
				case 1:
					printf("text\n");
					break;
					
				case 2:
					printf("data\n");
					break;
					
				case 3:
					printf("bss\n");
					break;
					
				default:
					printf("abs\n");
					break;
			}
		}
	}
	
	// being outputting linked object file
	aout = xfopen("ldout.tmp", "wb");
	
	// emit the head
	emhead();
	
	// close and move output file
	xfclose(aout);
	rename("ldout.tmp", "a.out");
} 