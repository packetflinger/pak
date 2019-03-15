#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <unistd.h>
#include <string.h>

// 1262698832 (0x4b434150)
#define MAGIC 	(('K' << 24) + ('C' << 16) + ('A' << 8) + 'P')

#define HEADERLEN		12
#define MAXFILES 		4096
#define MAXFILENAME		64	

#define true 	1
#define false	0

#define OPT_VERBOSE		1<<0
#define OPT_LIST		1<<1
#define OPT_EXTRACT		1<<2
#define OPT_CREATE		1<<3

// a file in a pak archive
typedef struct {
	char name[56];
	uint32_t offset;
	uint32_t length;
} pak_file_t;

// the header of the pak archive
typedef struct {
	uint32_t offset;
	uint32_t length;
	uint32_t filecount;
} pak_header_t;

// the whole pak archive
typedef struct {
	pak_header_t header;
	pak_file_t *files;		//[MAXFILES]
	uint32_t position;
} pak_t;

uint32_t pos;
uint32_t options;

static _Bool usage(int32_t argc, char** argv) {
	if (argc < 2) {
		printf("Usage: %s <bspfile> [<bspfile>...]\n", argv[0]);
		return false;
	}

	return true;
}

uint32_t readLong(char *data, uint32_t offset) {
	return ((data[offset+3] & 0xff) << 24) + 
		((data[offset+2] & 0xff) << 16) + 
		((data[offset+1] & 0xff) << 8) + 
		(data[offset] & 0xff);
}

uint32_t ReadInt(char *data) {
	uint32_t i;
	i = ((data[pos+3] & 0xff) << 24) + 
		((data[pos+2] & 0xff) << 16) + 
		((data[pos+1] & 0xff) << 8) + 
		(data[pos] & 0xff);
	pos += 4;
	
	return i;
}	

void readString(char *data, size_t len, char *buf) {
	int32_t i;
	
	for (i = 0; i < len; i++) {
		buf[i] = data[i];
	}
}

void readData(char *data, size_t len, char *buf) {
	int32_t i;
	for (i = 0; i < len; i++) {
		buf[i] = data[pos];
		pos++;
	}
}

void hexDump (char *desc, void *addr, int len) {
    int i;
    unsigned char buff[17];
    unsigned char *pc = (unsigned char*)addr;

    // Output description if given.
    if (desc != NULL)
        printf ("%s:\n", desc);

    if (len == 0) {
        printf("  ZERO LENGTH\n");
        return;
    }
    if (len < 0) {
        printf("  NEGATIVE LENGTH: %i\n",len);
        return;
    }

    // Process every byte in the data.
    for (i = 0; i < len; i++) {
        // Multiple of 16 means new line (with line offset).

        if ((i % 16) == 0) {
            // Just don't print ASCII for the zeroth line.
            if (i != 0)
                printf ("  %s\n", buff);

            // Output the offset.
            printf ("  %04x ", i);
        }

        // Now the hex code for the specific character.
        printf (" %02x", pc[i]);

        // And store a printable ASCII character for later.
        if ((pc[i] < 0x20) || (pc[i] > 0x7e))
            buff[i % 16] = '.';
        else
            buff[i % 16] = pc[i];
        buff[(i % 16) + 1] = '\0';
    }

    // Pad out last line if not exactly 16 characters.
    while ((i % 16) != 0) {
        printf ("   ");
        i++;
    }

    // And print the final ASCII bit.
    printf ("  %s\n", buff);
}

// Get the size of a file on the disk (for adding to pak archive)
long getFileSize(const char *filename) {
	FILE *fp;
	long sz;
	
	fp = fopen(filename, "rb");
	if (!fp) {
		return 0L;
	}
	
	// move to the end and ask for the size
	fseek(fp, 0L, SEEK_END);
	sz = ftell(fp);
	
	fclose(fp);
	return sz;
}

_Bool parsePak(char *pakfilename, pak_t *p) {
	FILE *fp;
	char buffer[HEADERLEN];
	uint32_t i;
	
	fp = fopen(pakfilename, "rb");
	fread(buffer, sizeof(buffer[0]), HEADERLEN, fp);
	
	if (ReadInt(buffer) != MAGIC) {
		printf("Not a valid .pak file, exiting.\n");
		fclose(fp);
		return false;
	}
	
	p->header.offset = ReadInt(buffer);
	p->header.length = ReadInt(buffer);
	p->header.filecount = p->header.length / MAXFILENAME; 
	p->position = pos;

	p->files = malloc(p->header.filecount * sizeof(pak_file_t));
	memset(p->files, 0, sizeof(pak_file_t) * p->header.filecount);
	
	char namebuf[p->header.filecount * MAXFILENAME];
	fseek(fp, p->header.offset - 12, SEEK_SET);
	fread(namebuf, sizeof(namebuf[0]), p->header.length, fp);
	
	char name[MAXFILENAME];
	//int tmp;
	for (i=0; i<p->header.filecount; i++) {
		memset(name, 0, sizeof(name));
		readData(namebuf, MAXFILENAME, name);
		
		hexDump("name ", &name, sizeof(name));
		
		memcpy(&p->files[i].name, name, 56);
		
		p->files[i].offset = readLong(name, 56);
		p->files[i].length = readLong(name, 60);
	}

	fclose(fp);
	return true;
}

void listPakFiles(pak_t *p) {
	uint32_t i;
	
	for (i=0; i<p->header.filecount; i++) {
		printf("%s (%ld bytes) - loc: %d\n", p->files[i].name, p->files[i].length, p->files[i].offset);
	}
}


int32_t main(int32_t argc, char** argv) {
	
	/*if (!usage(argc, argv)){
		return EXIT_FAILURE;
	}*/

	pak_t pakfile;
	parsePak(argv[1], &pakfile);
	
	listPakFiles(&pakfile);
	
	if (pakfile.files) {
		free(pakfile.files);
	}
	
	//printf("%ld\n", getFileSize(argv[1]));
	
	return EXIT_SUCCESS;
}
