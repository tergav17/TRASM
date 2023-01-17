/*
 * ld.c
 *
 * link editor for the TRASM toolchain
 */
 
#include "ld.h"

#define VERSION "1.0"

/* buffers */
uint8_t header[16];

/* flags */
char flagv = 0;

/* tables */
struct object *obj_table;

/*
 * prints error message, and exits
 *
 * msg = error message
 */
void error(char *msg, char *issue)
{
	printf("error:");
	printf(msg, issue);
	printf("\n");
	exit(1);
}

/*
 * alloc memory and check for success
 *
 * s = size in bytes
 */
void *xalloc(int s)
{
	void *o;
	
	if (!(o = malloc(s))) {
		error("cannot alloc", NULL);
	}
	return o;
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
		error("cannot open", NULL);
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
	
	// read the header in
	f = xfopen(fname, "rb");
	fread(header, 16, 1, f);
	
	// start doing checking
	if (header[0x00] != 0x18 || header[0x01] != 0x0E)
		error("%s not an object file", fname);
	
	if (!(header[0x02] & 0x01))
		error("%s not linkable", fname);
	
	// read object text base
	obj->obj_base = rlend(&header[0x03]);
	
	// read sizes
	obj->text_size = rlend(&header[0x0A]);
	obj->data_size = rlend(&header[0x0C]) - obj->text_size;
	obj->bss_size = rlend(&header[0x0E]) - obj->text_size - obj->data_size;
	
	// add it to the table
	if (obj_table) {
		curr = obj_table;
		while (curr->next)
			curr = curr->next;
		curr->next = obj;
	} else {
		obj_table = obj;
	}
	
	fclose(f);
}

int main(int argc, char *argv[])
{
	int i;
	
	// flag switch
	for (i = 0; i < argc; i++) {
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
	
} 