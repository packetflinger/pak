#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>    // for memcpy
#include <getopt.h>
#include <stdbool.h>
#include <sys/stat.h>

// 0x4b434150
#define MAGIC           (('K' << 24) + ('C' << 16) + ('A' << 8) + 'P')

#define HEADERLEN       12
#define MAXFILES        4096
#define MAXFILENAME     64

#define OPT_VERBOSE     1<<0
#define OPT_LIST        1<<1
#define OPT_EXTRACT     1<<2
#define OPT_CREATE      1<<3
#define OPT_USAGE       1<<4

typedef unsigned char   byte;


// a file in a pak archive
typedef struct {
    char name[56];
    uint32_t offset;
    uint32_t length;
    bool legit;
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
    pak_file_t *files;        //[MAXFILES]
    uint32_t position;
    bool writeable;
} pak_t;


uint32_t pos;
uint32_t options;
uint32_t opt;

char *pakfilename;

pak_file_t *filestoadd;
uint16_t filestoaddcount;


/**
 * Show how to use this problem
 */
static void usage(char *name) {
    printf("Usage: %s [-lxcf] packfile <file> [<file> ...]\n", name);
}

/**
 * Read 4 bytes of binary data from the file as a single number
 */
uint32_t readLong(char *data, uint32_t offset) {
    return ((data[offset+3] & 0xff) << 24) + 
        ((data[offset+2] & 0xff) << 16) + 
        ((data[offset+1] & 0xff) << 8) + 
        (data[offset] & 0xff);
}

/**
 * Write a 32bit integer as 4 bytes in the buffer
 */
byte *writeLong(uint32_t c){
    static byte buf[4];

    buf[0] = c & 0xff;
    buf[1] = (c >> 8) & 0xff;
    buf[2] = (c >> 16) & 0xff;
    buf[3] = c >> 24;

    return buf;
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

/**
 * Read a string
 */
void readString(char *data, size_t len, char *buf) {
    int32_t i;
    for (i = 0; i < len; i++) {
        buf[i] = data[i];
    }
}

/**
 * Read an arbitrary amount of data
 */
void readData(char *data, size_t len, char *buf) {
    int32_t i;
    for (i = 0; i < len; i++) {
        buf[i] = data[pos];
        pos++;
    }
}

/*
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
*/


/**
 * Get the size of a file on the disk (for adding to pak archive)
 */
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

/**
 * Load the pak
 */
bool parsePak(char *pakfilename, pak_t *p) {
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

    for (i=0; i<p->header.filecount; i++) {
        memset(name, 0, sizeof(name));
        readData(namebuf, MAXFILENAME, name);
        
        //hexDump("name ", &name, sizeof(name));
        
        memcpy(&p->files[i].name, name, 56);
        
        p->files[i].offset = readLong(name, 56);
        p->files[i].length = readLong(name, 60);
    }

    fclose(fp);
    return true;
}


/**
 * Output the files in the pak file to stdout
 */
void listPakFiles(pak_t *p) {
    uint32_t i;
    
    for (i=0; i<p->header.filecount; i++) {
        printf("%s (%ld bytes) - loc: %d\n", p->files[i].name, p->files[i].length, p->files[i].offset);
    }
}


/**
 * Figure out what we're supposed to do
 */
bool parseArgs(uint32_t argc, char **argv) {

    uint16_t i;
    options = 0;

    while ((opt = getopt(argc, argv, ":if:lrxhc")) != -1)
    {
        switch(opt)
        {
            case 'l':
                options |= OPT_LIST;
                break;
            case 'v':
                options |= OPT_VERBOSE;
            case 'x':
                //printf("option: %c\n", opt);
                options |= OPT_EXTRACT;
                break;
            case 'c':
                options |= OPT_CREATE;
                options &= ~OPT_LIST & ~OPT_EXTRACT;    // undo these
                filestoaddcount = 0;
                break;
            case 'h':
                options = 0;
                options |= OPT_USAGE;
                break;
            case 'f':
                //printf("filename: %s\n", optarg);
                pakfilename = optarg;
                break;
            case ':':
                printf("option needs a value\n");
                break;
            case '?':
                printf("unknown option: %c\n", optopt);
                break;
        }
    }

    if (options & OPT_CREATE) {
        //printf("recording files\n");
        filestoadd = malloc(sizeof(pak_file_t) * argc);
        memset(filestoadd, 0, sizeof(pak_file_t) * argc);

        for(i=0; optind < argc; optind++, i++){
            //intf("extra arguments: %s\n", argv[optind]);
            memcpy(filestoadd[i].name, argv[optind], 56);
            //printf("%s\n", filestoadd[i].name);
            filestoaddcount++;
        }
    }

    return true;
}


/**
 * Make a new pak file
 */
void createPak(pak_t *pak, pak_file_t *files) {
    struct stat fs;
    uint16_t i, totallegit = 0;
    size_t totalsz = 0;
    FILE *fp, *fp_src;
    byte *data;
    char name[MAXFILENAME];

    data = malloc(1);

    // validate all supplied files getting required data (size, etc)
    for (i=0; i<filestoaddcount; i++) {
        //printf("%s\n", files[i].name);
        if (stat(files[i].name, &fs) < 0) {
            printf("'%s' - file not found\n", files[i].name);
            continue;
        }

        files[i].length = fs.st_size;
        files[i].offset = totalsz;
        files[i].legit = true;

        totalsz += files[i].length;
        totallegit++;
        //printf("%s - %ld bytes\n", files[i].name, fs.st_size);
    }

    // make sure the target file doesn't already exist
    if (!stat(pakfilename, &fs)) {
        printf("%s already exists\n");
        return;
    }

    fp = fopen(pakfilename, "wb");

    fwrite(writeLong(MAGIC), 1, 4, fp);
    fwrite(writeLong(totalsz), 1, 4, fp);
    fwrite(writeLong(totallegit * MAXFILENAME), 1, 4, fp);

    // write actual file data
    for (i=0; i<filestoaddcount; i++) {
        if (!files[i].legit) {
            continue;
        }

        data = realloc(data, files[i].length);
        fp_src = fopen(files[i].name, "rb");
        fread(data, 1, files[i].length, fp_src);
        fwrite(data, 1, files[i].length, fp);

        fclose(fp_src);
        fflush(fp);
    }

    // write the file metadata
    for (i=0; i<filestoaddcount; i++) {
        if (!files[i].legit) {
            continue;
        }

        memset(&name, 0, MAXFILENAME);
        memcpy(&name, files[i].name, 56);
        //memcpy(&name[56], files[i].offset, 4);
        //memcpy(&name[60], files[i].length, 4);

        fwrite(&name, MAXFILENAME - 8, 1, fp);
        fwrite(writeLong(files[i].offset), 1, 4, fp);
        fwrite(writeLong(files[i].length), 1, 4, fp);
        fflush(fp);
    }

    fclose(fp);
}

int32_t main(int32_t argc, char** argv) {
    
    pak_t pakfile;

    if (!parseArgs(argc, argv)) {
        return EXIT_FAILURE;
    }

    if (!options || options & OPT_USAGE) {
        usage(argv[0]);
        return EXIT_SUCCESS;
    }

    /*
    if (!pakfilename[0]) {
        printf("error: no pak file specified, use -f <pakname>\n");
        return EXIT_SUCCESS;
    }
    */

    if (options & OPT_LIST) {
        parsePak(pakfilename, &pakfile);
        listPakFiles(&pakfile);
    }
    
    if (options & OPT_CREATE) {
        createPak(&pakfile, filestoadd);
    }
    
    return EXIT_SUCCESS;
}
