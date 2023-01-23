/*
 * ld.c
 *
 * link editor for the TRASM toolchain
 */
 
#include "ld.h"

#define VERSION "1.0"

/* buffers */
uint8_t header[16];
uint8_t tmp[16];

/* flags */
char flagv = 0;
char flagr = 0;
char flags = 0;

/* tables */
struct object *obj_table;
struct object *obj_tail;

struct reloc *reloc_table;

struct extrn *ext_table;
struct extrn *ext_tail;

struct archive *arc_table;
struct archive *arc_tail;

/* output binary stuff */
FILE *aout;

/* state variables */
char newext; 

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
 * open object file and check for success
 *
 * obj = object struct to open
 * returns pointer to file
 */
FILE *xoopen(struct object *obj)
{
	FILE *f;
	
	f = xfopen(obj->fname, "rb");
	xfseek(f, obj->offset, SEEK_CUR);
	
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
 * checks if two arrays are equal
 *
 * a = pointer to array a
 * b = pointer to array b
 * len = length in bytes
 * returns 1 if equal
 */
char aequ(char *a, char *b, int len)
{
	while (len--) {
		if (*a != *b)
			return 0;
		a++;
		b++;
	}
	
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
 * allocs a new reloc struct and inits it
 *
 * returns new object
 */
struct reloc *nreloc()
{
	struct reloc *new;
	int i;
	
	// allocate start of relocation table
	new = (struct reloc *) xalloc(sizeof(struct reloc));
	for (i = 0; i < RELOC_SIZE; i++) new->addr[i] = 255;
	new->next = NULL;
	
	return new;
}

/*
 * allocs a new patch struct and inits it
 *
 * returns new object
 */
struct patch *npatch()
{
	struct patch *new;
	int i;
	
	// allocate patch struct and init
	new = (struct patch *) xalloc(sizeof(struct patch));
	for (i = 0; i < PATCH_SIZE; i++) new->addr[i] = 0;
	new->next = NULL;
	
	return new;
}


/*
 * insert an address into a reloc table
 *
 * tab = relocation table
 * target = address to add
 */
void reloci(struct reloc *tab, uint16_t target)
{
	struct reloc *curr;
	uint16_t last, diff;
	uint8_t next, tmp;
	int i;
	
	// begin at table start
	curr = tab;
	last = 0;
	i = 0;
	next = 255;
	// find end of table
	while (curr->addr[i] != 255 && next == 255) {
		
		if (target <= curr->addr[i] + last) {
			diff = target - last;
			next = curr->addr[i] - diff;
			curr->addr[i] = diff;
		}
		
		last += curr->addr[i];
		i++;
		
		// advance to next record
		if (i >= RELOC_SIZE) {
			curr = curr->next;
			i = 0;
		}
	}
	
	// do forwarding if needed
	while (curr->addr[i] != 255) {
		
		// swap
		tmp = curr->addr[i];
		curr->addr[i] = next;
		next = tmp;
		
		last += curr->addr[i];
		i++;
		
		// advance to next record
		if (i >= RELOC_SIZE) {
			curr = curr->next;
			i = 0;
		}
	}
	
	// diff the difference
	if (next == 255) diff = target - last;
	else diff = next;
	do {
		// calculate next byte to add
		if (diff < 254) {
			next = diff;
			diff = 0;
		} else {
			diff = diff - 254;
			next = 254;
		}
		
		// add it to the reloc table, extending a record if needed
		curr->addr[i++] = next;
		if (i >= RELOC_SIZE) {
			i = 0;
			curr->next = nreloc();
			curr = curr->next;
		}
	} while (next == 254);
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
 * adds an object to the object table
 *
 * new = new object
 */
void addobj(struct object *new)
{	
	// add it to the table
	if (obj_table) {
		obj_tail->next = new;
	} else {
		obj_table = new;
	}
	obj_tail = new;
}

/*
 * adds an archive to the archive table
 *
 * new = new archive
 */
void addarc(char *fname)
{
	struct archive *new;
	new = (struct archive *) xalloc(sizeof(struct archive));
	
	// add it to the table
	if (arc_table) {
		arc_tail->next = new;
	} else {
		arc_table = new;
	}
	arc_tail = new;
}


/*
 * checks if a file is an archive or not
 */
char isarch(char *fname)
{
	FILE *f;
	
	f = xfopen(fname, "rb");
	fread(header, 8, 1, f);
	xfclose(f);
	
	if (aequ((char *) header, "!<arch>\n", 8)) {
		return 1;
	}
	return 0;
}

/*
 * skips over a relocation or symbol seg
 */
void skipsg(FILE *f)
{
	uint8_t b[2];
	
	fread(b, 2, 1, f);
	xfseek(f, rlend(b), SEEK_CUR);
}

/*
 * generates an external prototype if it doesn't already exist
 *
 * name = name of external
 */
void extprot(char *name)
{
	struct extrn *curr;
	
	for (curr = ext_table; curr; curr = curr->next) {
		// if the prototype already exists, don't bother making a new one
		if (sequ(curr->name, name))
			return;
	}
	
	// new extern prototype
	
	// alloc new struct
	newext = 1;
	curr = (struct extrn *) xalloc(sizeof(struct extrn));
	memcpy(curr->name, name, SYMBOL_NAME_SIZE);
	curr->next = NULL;
	curr->patch = npatch();
	
	curr->value = 0;
	curr->type = 0;
	curr->source = NULL;
	
	// add it to the table
	if (ext_table) {
		ext_tail->next = curr;
	} else {
		ext_table = curr;
	}
	ext_tail = curr;
}

/*
 * returns an object based on a filename and index
 *
 * fname = file name
 * index = index
 * returns object if found, null if not
 */
struct object *getobj(char *fname, uint8_t index)
{
	struct object *obj;
	
	for (obj = obj_table; obj; obj = obj->next)
		if (obj->fname == fname && obj->index == index)
			break;
		
	return obj;
}

/*
 * returns an extrn based on name
 *
 * name = extrn name
 * returns extrn if found, null if not
 */
struct extrn *getext(char *name)
{
	// see if there is an external to check in
	for (ext = ext_table; ext; ext = ext->next)
		if (sequ(ext->name, (char *) tmp))
			break;
		
	return ext;
}

/*
 * checks in an object file and adds it to the table
 *
 * fname = path to object file
 * index = record index if archive
 */
void chkobj(char *fname, uint8_t index)
{
	FILE *f;
	struct object *obj;
	int offset;
	
	// do a quick check to make sure this hasn't already been checked in
	if (getobj(fname, index))
		return;
	
	// alloc object
	obj = (struct object *) xalloc(sizeof(struct object));
	obj->next = NULL;
	obj->index = index;
	obj->fname = fname;
	
	// read the header in
	f = xfopen(fname, "rb");
	fread(header, 8, 1, f);
	
	// check if it is an archive or not
	if (aequ((char *) header, "!<arch>\n", 8)) {
		while (index) {
			// skip to next record
			xfseek(f, 48, SEEK_CUR);
			
			// read in file size
			fread(tmp, 10, 1, f);
			offset = atoi((char *) tmp);
			
			// on to next record
			// on the real system, this will need to be broken up for >32kb files
			xfseek(f, offset + 2, SEEK_CUR);
			index--;
		}
		
		// seek at start of record
		xfseek(f, 60, SEEK_CUR);
		
		// read in header
		fread(header, 16, 1, f);
	} else {
		// read in the rest of the header
		fread(header+8, 8, 1, f);
	}
	
	// set offset
	obj->offset = ftell(f) - 16;
	
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
	
	// add to object table
	addobj(obj);
	
	// next we will dump out the external symbols
	xfseek(f, rlend(&header[0x0C]) - 16, SEEK_CUR);
	
	// skip over relocations and symbols
	skipsg(f);
	skipsg(f);
	
	// dump everything out
	while (fread(tmp, SYMBOL_NAME_SIZE-1, 1, f) > 0 && tmp[0]) {
		tmp[SYMBOL_NAME_SIZE] = 0;
		extprot((char *) tmp);
		skipsg(f);
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

/*
 * clears the symbol table before a symdump cycle
 * used in making sure there aren't any duplicate symbols linked
 */
void sclear()
{
	struct extrn *curr;
	
	for (curr = ext_table; curr; curr = curr->next)
		curr->source = NULL;
}

/*
 * dumps out all the symbols in an archive or object file
 *
 * fname = file name
 * index = index of archive
 *
 * returns 1 if has next object in archive
 */
char sdump(char *fname, uint8_t index)
{
	uint16_t nsym;
	uint8_t b[2], type, cnt;
	FILE *f;
	struct extrn *ext;
	struct object *obj;
	int offset;
	char ret, dochk;
	
	// read the header in
	f = xfopen(fname, "rb");
	fread(header, 8, 1, f);
	
	// check if it is an archive or not
	ret = 1;
	if (aequ((char *) header, "!<arch>\n", 8)) {
		cnt = index;
		while (cnt) {
			// skip to next record
			xfseek(f, 48, SEEK_CUR);
			
			// read in file size
			fread(tmp, 10, 1, f);
			offset = atoi((char *) tmp);
			
			// on to next record
			// on the real system, this will need to be broken up for >32kb files
			xfseek(f, offset + 2, SEEK_CUR);
			cnt--;
		}
		
		// ensure that there is a record here
		if (fread(tmp, 16, 1, f) != 1) {
			xfclose(f);
			return 0;
		}
		tmp[15] = 0;
		
		// seek at start of record
		xfseek(f, 44, SEEK_CUR);
		
		// read in header
		fread(header, 16, 1, f);
	} else {
		// no more records to read after this
		ret = 0;
		
		// read in the rest of the header
		fread(header+8, 8, 1, f);
	}
	
		// start doing checking
	if (header[0x00] != 0x18 || header[0x01] != 0x0E)
		error("%s not an object file", fname);
	
	// now  we will dump out the internal symbols
	xfseek(f, rlend(&header[0x0C]) - 16, SEEK_CUR);
	
	// read the number of symbols
	skipsg(f);
	fread(b, 2, 1, f);
	nsym = rlend(b) / SYMBOL_REC_SIZE;
	
	// grab the object
	dochk = 0;
	obj = getobj(fname, index);
	
	while (nsym--) {
		// read name
		fread(tmp, SYMBOL_NAME_SIZE-1, 1, f);
		tmp[SYMBOL_NAME_SIZE] = 0;
		
		// read type and value
		fread(&type, 1, 1, f);
		fread(b, 2, 1, f);
		
		ext = getext((char *) tmp);
		
		// if an external can't be found, move on to the next symbol
		if (!ext)
			continue;
		
		dochk = 1;
		
		// make sure this symbol hasn't been checked in already
		if (ext->source)
			error("duplicate symbol %s", (char *) tmp);
		
		// update it
		ext->value = rlend(b);
		ext->type = type;
		ext->source = obj;
		
		// if this object is undefined, we will need to run another pass to grab it
		if (!obj)
			newext = 1;
	}
	
	xfclose(f);
	
	// if the object hasn't been checked in and needs to, check it in
	if (!obj && dochk) 
		chkobj(fname, index);
	
	return ret;
}

/*
 * dumps out external patch tables from an object
 *
 * obj = object to dump
 * seg = segment to dump (0 = text, 1 = data)
 */
void pdump(struct object *obj, uint8_t seg)
{
	FILE *f;
	uint16_t npat;
	
	// open object
	f = xoopen(obj);
	
	// read header and skip to extrn segment
	xfread(header, 16, 1, f);
	xfseek(f, rlend(&header[0x0C]) - 16, SEEK_CUR);
	skipsg(f);
	skipsg(f);
	
	// dump out all symbols
	while (1) {
		if (fread(tmp, SYMBOL_NAME_SIZE-1, 1, f) != 1 || !tmp[0])
			break;
		
		fread(b, 2, 1, f);
		npat = rlend(b) / 2;
	}
	
	xfclose(f);
}

/*
 * fixes symbols based on what segment they are in
 * this is to be run after bases are computed
 */
void sfix()
{
	struct extrn *curr;
	uint16_t value;
	
	// fix every symbol
	for (curr = ext_table; curr; curr = curr->next) {
		
		// don't fix undefined externals
		if (!curr->source)
			continue;
		
		value = curr->value;
		// fix segments
		if (curr->type != 4) {
			// relocate to address 0
			value -= curr->source->org + 16;
			
			switch (curr->type) {
				
				case 0:
					error("symbol %s is undefined", curr->name);
				
				case 1:
					value = value + curr->source->text_base;
					break;
					
				case 2:
					value -= curr->source->text_size;
					value +=  curr->source->data_base;
					break;
					
				case 3:
					value -= curr->source->text_size + curr->source->data_size;
					value += curr->source->bss_base;
					break;
					
				default:
					error("symbol %s is external", curr->name);
			}
		}
		curr->value = value;
	}
}

int main(int argc, char *argv[])
{
	int i, o, udcnt;
	struct object *obj;
	struct extrn *ext;
	
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
	
	// first we set up the external tables
	newext = 0;
	
	// check in all object files (but not archives)
	for (i = 1; i < argc; i++) {
		if (argv[i][0] != '-') {
			if (isarch(argv[i])) {
				// add it to the archive table for later
				addarc(argv[i]);
			} else {
				chkobj(argv[i], 0);
			}
		}
	}
	
	// dump symbols
	while (newext) {
		// if we link in any new externals, we run the loop again
		sclear();		
		newext = 0;
		for (i = 1; i < argc; i++) {
			if (argv[i][0] != '-') {
				o = 0;
				while (sdump(argv[i], o++));
			}
		}
	}
	
	// check for undefined external 
	udcnt = 0;
	for (ext = ext_table; ext; ext = ext->next) {
		if (!ext->source) {
			if (!udcnt) {
				printf("undefined:\n");
			}
			udcnt++;
			printf("%s\n", ext->name);
		}
	}
	if (udcnt)
		error("undefined externals", NULL);
	
	// calculate bases / fix symbols
	cmbase();
	sfix();
	
	// print out objects
	if (flagv) {
		printf("object file base/size:\n");
		for (obj = obj_table; obj; obj = obj->next) {
			printf("	text: %04x:%04x, data: %04x:%04x, bss: %04x:%04x <- %s,%d\n", obj->text_base, obj->text_size, obj->data_base, obj->data_size, obj->bss_base, obj->bss_size, obj->fname, obj->index);
		}
	}
	
	// print out symbols
	if (flagv) {
		printf("symbol name/value/segment\n");
		for (ext = ext_table; ext; ext = ext->next) {
			printf("	name: %s, value: %04x ", ext->name, ext->value);
			switch(ext->type) {
				case 0:
					printf("undef\n");
					break;
				
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
	
	
	// begin outputting linked object file
	aout = xfopen("ldout.tmp", "wb");
	
	// emit the head
	emhead();
	
	// close and move output file
	xfclose(aout);
	rename("ldout.tmp", "a.out");
} 