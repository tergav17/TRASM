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

/* tables */
struct object *obj_table;
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
	
	xfclose(f);
}

void sdump(struct object *obj)
{
	FILE *f;
	struct symbol *sym, *curr;
	int nsym;
	
	// read the header in
	f = xfopen(obj->fname, "rb");
	fread(header, 16, 1, f);
	
	// navigate to start of symbol table
	xfseek(f, rlend(&header[0x0E]), SEEK_SET);
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
		sym->value = rlend(tmp+1);
		
		// check for issues in the symbol
		if (!sym->type)
			error("symbol %s is undefined", sym->name);
		
		if (sym->type > 4)
			error("symbol %s is external", sym->name);
		
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

int main(int argc, char *argv[])
{
	int i;
	struct object *curr;
	struct symbol *scurr;
	
	// flag switch
	for (i = 1; i < argc; i++) {
		if (argv[i][0] == '-') {
			switch (argv[i][1]) {
					
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
			printf("	%-8s %d:%04x\n", scurr->name, scurr->type, scurr->value);
		}
	}
	
	
} 