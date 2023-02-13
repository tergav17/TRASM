/*
 * ld.c
 *
 * link editor for the TRASM toolchain
 */
 
#include "ld.h"

#define VERSION "1.0"

/* buffers */
uint8_t header[16];
uint8_t tmp[512];

/* flags */
char flagv = 0;
char flagr = 0;
char flags = 0;

/* tables */
struct object *obj_table;
struct object *obj_tail;

struct extrn *ext_table;
struct extrn *ext_tail;

struct archive *arc_table;
struct archive *arc_tail;

/* output binary stuff */
FILE *aout;

/* relocation temp file */
FILE *ltmp;
char tname[32];

/* state variables */
char newext; 

/* stream stuff */
FILE *relf;
uint16_t nreloc;

/* record keeping */
uint16_t reloc_rec;
uint16_t glob_rec;
uint16_t extrn_rec;

uint8_t extn;

/* link address */
uint16_t laddr;

/* arg zero */
char *argz;


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
		remove(TMP_FILE);
	}
	// linking failed, remove relocation temp file
	if (ltmp) {
		fclose(ltmp);
		remove(tname);
	}
	
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
void skipsg(FILE *f, uint8_t size)
{
	uint8_t b[2];
	
	fread(b, 2, 1, f);
	xfseek(f, rlend(b) * size, SEEK_CUR);
}

/*
 * returns an extrn based on name
 *
 * name = extrn name
 * returns extrn if found, null if not
 */
struct extrn *getext(char *name)
{
	struct extrn *ext;
	
	// see if there is an external to check in
	for (ext = ext_table; ext; ext = ext->next)
		if (sequ(ext->name, (char *) tmp))
			break;
		
	return ext;
}

/*
 * returns an extern based on a number
 *
 * number = external number
 * obj = object to search
 */
struct extrn *getref(uint8_t number, struct object *obj)
{
	struct reference *ref;
	
	// look for the reference
	for (ref = obj->head; ref; ref = ref->next)
		if (ref->number == number)
			return ref->ext;
		
	return NULL;
}

/*
 * generates an external prototype if it doesn't already exist
 *
 * obj = object source of record
 * record = pointer to record 
 */
void extprot(struct object *obj, uint8_t *record)
{
	struct extrn *ext;
	struct reference *ref;
	uint8_t number;
	
	// make sure number is external
	number = record[SYMBOL_NAME_SIZE-1];
	if (number < 5) {
		// record that a global symbol has been checked in
		glob_rec++;
		
		return;
	}
	
	// terminate symbol name and search
	record[SYMBOL_NAME_SIZE-1] = 0;
	ext = getext((char *) tmp);
		
	
	// new extern prototype?
	if (!ext) {
		// alloc new struct
		ext = (struct extrn *) xalloc(sizeof(struct extrn));
		memcpy(ext->name, record, SYMBOL_NAME_SIZE);
		newext = 1;
		
		ext->next = NULL;
		
		ext->value = 0;
		ext->type = 0;
		ext->source = NULL;
		
		ext->number = 0;
		
		// add it to the table
		if (ext_table) {
			ext_tail->next = ext;
		} else {
			ext_table = ext;
		}
		ext_tail = ext;
	}
	
	// now attach it to the object
	ref = (struct reference *) xalloc(sizeof(struct reference));
	ref->ext = ext;
	ref->number = number;
	
	if (obj->head) {
		obj->tail->next = ref;
	} else {
		obj->head = ref;
	}
	obj->tail = ref;
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
 * checks in an object file and adds it to the table
 *
 * fname = path to object file
 * index = record index if archive
 * returns pointer to new object struct
 */
struct object *chkobj(char *fname, uint8_t index)
{
	FILE *f;
	struct object *obj;
	uint8_t b[2];
	uint16_t nsym;
	int offset;
	
	// do a quick check to make sure this hasn't already been checked in
	if ((obj = getobj(fname, index)))
		return obj;
	
	// alloc object
	obj = (struct object *) xalloc(sizeof(struct object));
	obj->next = NULL;
	obj->head = NULL;
	obj->tail = NULL;
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
			
			// if the offset is odd, make it even
			if (offset % 2) offset++;
			
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
	
	if (!(header[0x02] & 0b01))
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
	
	// skip over relocations
	skipsg(f, RELOC_REC_SIZE);
	
	// read number of symbols
	fread(b, 2, 1, f);
	nsym = rlend(b);
	
	// dump everything out
	while (nsym--) {
		fread(tmp, SYMBOL_REC_SIZE, 1, f);
		extprot(obj, tmp);
	}
	
	// close and return
	xfclose(f);
	
	return obj;
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
	if (extn > 5) {
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
	struct extrn *ext;
	
	for (ext = ext_table; ext; ext = ext->next)
		ext->source = NULL;
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
	int offset;
	char ret;

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
			
			// if the offset is odd, make it even
			if (offset % 2) offset++;

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
	skipsg(f, RELOC_REC_SIZE);
	fread(b, 2, 1, f);
	nsym = rlend(b);
	
	while (nsym--) {
		// read name
		fread(tmp, SYMBOL_NAME_SIZE-1, 1, f);
		tmp[SYMBOL_NAME_SIZE-1] = 0;
		
		// read type and value
		fread(&type, 1, 1, f);
		fread(b, 2, 1, f);
		
		// if the type is external, skip it for now
		if (type > 4)
			continue;

		ext = getext((char *) tmp);
		
		// if an external can't be found, move on to the next symbol
		if (!ext)
			continue;

		// make sure this symbol hasn't been checked in already
		if (ext->source)
			error("duplicate symbol %s", (char *) tmp);
		
		// update it
		ext->value = rlend(b);
		ext->type = type;
		
		// set the source, checking it in if required
		ext->source = chkobj(fname, index);;
	}
	
	xfclose(f);
	
	return ret;
}

/*
 * relocates a symbol value to its correct place in the relocated object
 *
 * value = symbol value
 * type = symbol type
 * obj = source object
 */
uint16_t sreloc(uint16_t value, uint8_t type, struct object *obj)
{
	// if it is absolute, do nothing
	if (type == 4)
		return value;
	
	// relocate to address 0
	value -= obj->org;
	
	// this is a header value, don't relocate
	if (value < 0x10)
		return value;
	
	// pretend header doesn't exist
	value -= 16;
	
	switch (type) {
		
		case 0:
			error("undefined symbol", NULL);
		
		case 1:
			return value + obj->text_base;
			
		case 2:
			value -= obj->text_size;
			return value + obj->data_base;
			
		case 3:
			value -= obj->text_size + obj->data_size;
			return value + obj->bss_base;
			
		default:
			error("external symbol", NULL);
	}
	
	// we shouldn't be able to get here
	return 0;
}

/*
 * fixes external symbols based on what segment they are in
 * this is to be run after bases are computed
 */
void sfix()
{
	struct extrn *ext;
	
	// fix every symbol
	for (ext = ext_table; ext; ext = ext->next) {
		
		// don't fix undefined externals
		if (!ext->source)
			continue;
		
		ext->value = sreloc(ext->value, ext->type, ext->source);
	}
}

/*
 * copy all symbols from source objects to final object
 */
void scopy()
{
	struct object *obj;
	uint16_t nsym, value;
	FILE *f;
	
	for (obj = obj_table; obj; obj = obj->next) {
		f = xoopen(obj);
		
		// jump to symbol table
		fread(header, 16, 1, f);
		xfseek(f, rlend(&header[0x0C]) - 16, SEEK_CUR);
		skipsg(f, RELOC_REC_SIZE);
		
		fread(tmp, 2, 1, f);
		nsym = rlend(tmp);
		
		while (nsym--) {
			// read in symbol record
			fread(tmp, SYMBOL_REC_SIZE, 1, f);
			
			// skip external symbols
			if (tmp[SYMBOL_REC_SIZE-3] > 4)
				continue;
			
			value = rlend(&tmp[SYMBOL_REC_SIZE-2]);
			
			// relocate value to proper place
			value = sreloc(value, tmp[SYMBOL_REC_SIZE-3], obj);
			
			// write it out
			wlend(&tmp[SYMBOL_REC_SIZE-2], value);
			fwrite(tmp, SYMBOL_REC_SIZE, 1, aout);
		}
		
		xfclose(f);
	}
	
	// copy all undefined externals back into the binary
	
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
void sopen(struct object *obj)
{
	// open objects
	relf = xoopen(obj);
	
	// first one jumps to relocation table
	fread(header, 16, 1, relf);
	xfseek(relf, rlend(&header[0x0C]) - 16, SEEK_CUR);
	
	// now we read in the number of records for each
	fread(tmp, 2, 1, relf);
	nreloc = rlend(tmp);
}

/*
 * reads the next value in the stream into a tval
 *
 * out = pointer to output tval
 */
void snext(struct tval *out)
{
	if (nreloc) {
		nreloc--;
		fread(tmp, 3, 1, relf);
		out->type = tmp[0];
		out->value = rlend(tmp + 1);
	} else {
		out->value = 0;
		out->type = 0;
	}
	

}

/*
 * emits a binary segment
 *
 * obj = object to emit
 * seg = segment to emit (0 = text, 1 = data)
 */
void emseg(struct object *obj, uint8_t seg)
{
	FILE *bin;
	struct tval next;
	struct extrn *ext;
	uint16_t skip, last, chunk, left, value;
	
	// open up the object binary and stream
	bin = xoopen(obj);
	sopen(obj);
	
	// first we figure out how much information to skip
	// header always gets skipped
	skip = 0x10;
	if (seg) {
		// skip text segment too
		left = obj->data_size;
		skip += obj->text_size;
	} else {
		left = obj->text_size;
	}
	
	// seek binary, and scan through stream
	xfseek(bin, skip, SEEK_CUR);
	for (snext(&next); next.value && next.value < skip; snext(&next));
	
	last = skip;
	// read out the binary
	while (left) {
		
		// figure out how many bytes to read this chunk
		if (next.value) {
			// sanity check
			if (last > next.value) {
				error("backwards relocation", NULL);
			}
			
			chunk =  next.value - last;
		} else 
			chunk = left;
		
		// make sure we don't overdo it
		if (chunk > left)
			chunk = left;
		
		// make sure we aren't reading more than 512 bytes
		if (chunk > 512)
			chunk = 512;
		
		// transfer to binary
		fread(tmp, chunk, 1, bin);
		fwrite(tmp, chunk, 1, aout);
		left -= chunk;
		last += chunk;
		laddr += chunk;
		
		// see if we should do a relocation
		if (next.value == last && left) {
			if (left < 2)
				error("cannot relocate byte", NULL);
			
			// read 2 bytes
			fread(tmp, 2, 1, bin);
			value = rlend(tmp);
			
			if (next.type > 0 && next.type < 4) {
				// relocate to segment
				value = sreloc(value, next.type, obj);
				
				reloc_rec++;
				tmp[0] = next.type;
				wlend(tmp+1, laddr);
				fwrite(tmp, RELOC_REC_SIZE, 1, ltmp);
			} else {
				// relocate to external
				ext = getref(next.type, obj);
				if (!ext)
					error("invalid external number", NULL);
				
				// check to see if this is a valid external or not
				if (ext->number) {
					// its undefined, leave the value unchanged and record this in relocations
					reloc_rec++;
					tmp[0] = ext->number;
					wlend(tmp+1, laddr);
					fwrite(tmp, RELOC_REC_SIZE, 1, ltmp);
				} else {
					// its defined, patch in external symbol
					value += ext->value;
					
					if (ext->type > 0 && ext->type < 4) {
						reloc_rec++;
						tmp[0] = ext->type;
						wlend(tmp+1, laddr);
						fwrite(tmp, RELOC_REC_SIZE, 1, ltmp);
					}
				}
			}
			
			// write the binary
			wlend(tmp, value);
			fwrite(tmp, 2, 1, aout);
			
			// update trackers
			left -= 2;
			last += 2;
			laddr += 2;
			
			// grab next
			snext(&next);
		}
	}
	
	
	// close everything
	xfclose(bin);
	sclose();
}

/*
 * emits the binary section of the object file
 */
void embin()
{
	uint8_t seg;
	struct object *obj;
	
	// count number of relocations in final binary
	reloc_rec = 0;
	
	// relocation starts at 0x10
	laddr = 0x10;
	
	// first the text segment is emitted, then data
	for (seg = 0; seg < 2; seg++) {
		// objects will be emitted in order
		for (obj = obj_table; obj; obj = obj->next) {
			emseg(obj, seg);
		}
	}
}

/*
 * print usage message
 */
void usage()
{
	printf("usage: %s [-vs] [-r] object.o ...\n", argz);
	exit(1);
}

int main(int argc, char *argv[])
{
	int i, o;
	struct object *obj;
	struct extrn *ext;
	
	argz = argv[0];
	
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
						usage();
						break;
				}
				o++;
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
	
	// check for invalid configurations
	if (flags && flagr)
		usage();
	
	
	// intro message
	if (flagv)
		printf("TRASM link editor v%s\n", VERSION);
	
	// first we set up the external tables
	newext = 0;
	
	// keep track of all globals in checked in objects
	glob_rec = 0;
	
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
	
	// reset record keeping
	extrn_rec = 0;
	
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
	extn = 5;
	
	// either list undefined external, or number it
	for (ext = ext_table; ext; ext = ext->next) {
		if (!ext->source) {
			extrn_rec++;
			if (!extn)
				error("out of externals", NULL);
			if (!flagr) {
				if (extn == 5) 
					printf("undefined:\n");
				printf("%s\n", ext->name);
			} else {
				ext->number = extn;
			}
			extn++;
		}
	}
	if (extn != 5 && !flagr)
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
	aout = xfopen(TMP_FILE, "wb");
	
	// emit the head
	emhead();
	
	// open temp file
	sprintf(tname, "/tmp/ltm%d", getpid());
	ltmp = xfopen(tname, "wb");
	
	// emit the binary contents
	embin();
	
	// write relocation header / data
	wlend(tmp, ++reloc_rec);
	fwrite(tmp, 2, 1, aout);
	
	// append contents of temp file to output
	xfclose(ltmp);
	ltmp = xfopen(tname, "rb");
	while ((i = fread(tmp, 1, 512, ltmp)))
		fwrite(tmp, 1, i, aout);
	xfclose(ltmp);
	remove(tname);
	
	// write terminator
	tmp[0] = tmp[1] = tmp[2] = 0;
	fwrite(tmp, 3, 1, aout);
	
	// if flags, do not write globs but do write externals
	if (flags) {
		// no globs
		glob_rec = 0;
	} 
	
	if (flagv)
		printf("symbol output:\n	global: %d\n	external: %d\n", glob_rec, extrn_rec);
		
	wlend(tmp, glob_rec + extrn_rec);
	fwrite(tmp, 2, 1, aout);
	
	if (!flags) {
		// write actual symbol table
		scopy();
	}
	
	// quickly copy over all externals
	for (ext = ext_table; ext; ext = ext->next) {
		if (ext->number) {
			fwrite(ext->name, SYMBOL_NAME_SIZE-1, 1, aout);
			tmp[0] = ext->number;
			tmp[2] = tmp[1] = 0;
			fwrite(tmp, 3, 1, aout);
		}
	}
	
	// close and move output file
	xfclose(aout);
	rename(TMP_FILE, "a.out");
} 