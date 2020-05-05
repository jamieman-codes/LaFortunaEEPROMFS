/* COMP2215  Task 4*/

#include "lcd.h"
#include "eepromfs.h"

#include <stdint.h>
#include <avr/io.h>
#include <avr/eeprom.h>

typedef uint8_t block;
typedef uint8_t filename;

typedef struct entry {
    block  startblock;
    uint16_t length;
    uint16_t currpos;
    uint8_t perms; /*0 = No perms, 1  = Open for Write, 2 = Open for Read, 3 = open for append */
} direntry;

typedef struct fat{
    block data;
    block nextLocation;
} fatS;

typedef struct meta {
    uint16_t version;
    direntry dir[MAX_FILES];   /* 5 B * 10 = 50 B */
    fatS fat[TOTALBLOCKS];    /* (1600 B) / (64 B/block) = 25 blocks */
} metadata;

metadata md;

int main(){
	init();
    /*
	open_for_write(1);
    uint8_t StringOfData[4] = "TEST";
    write(1, StringOfData, sizeof(StringOfData));
    close(1);
    open_for_write(2);
    write(2, StringOfData, sizeof(StringOfData));
    close(2);
    open_for_append(1);
    write(1, StringOfData, sizeof(StringOfData));
    close(1);
    open_for_read(1);
    read(1);
     */

    return 1;
}

void init(void) {
    /* 8MHz clock, no prescaling (DS, p. 48) */
    CLKPR = (1 << CLKPCE);
    CLKPR = 0;

    init_lcd();
	init_eepromfs();
}

void init_eepromfs() {
    filename fn;
    block bl;

    printf( "Dir entry size: %u\n", sizeof(direntry) ); /*size of 1 entry is 5 */
    printf( "Meta data size: %u\n", sizeof(metadata) ); /*Metadata size 77 */

    eeprom_read_block((void *) &md, (const void *) EEPROM_START, sizeof(md)); /*loads current EEPROM data into metadata format*/

    if(md.version == FSVERSION_UINT16) { /* if file systems exists*/
        printf("EEPROM Metadata detected\n");
        printf("File System version: %u\n", md.version);
        for(fn = 0; fn < MAX_FILES; fn++){
            md.dir[fn].currpos = CLOSED_FILE; /*Closes every file*/
            if(md.dir[fn].startblock != FREE_BLOCK){
                printf("File %d found. File length: %d\n", fn, md.dir[fn].length); /*Print out files found*/
            }
        }
    }

    else { /*if file system doesnt exist*/
        printf("No Metadata detected\n");
        md.version = FSVERSION_UINT16;
        for( fn = 0; fn < MAX_FILES ; fn++){ /*Create blank files for every available file on the system*/
            md.dir[fn].startblock = FREE_BLOCK; /* FREE_BLOCK is not a file */
            md.dir[fn].length = 0;
            md.dir[fn].currpos = CLOSED_FILE; /*When a read is attempted the file will show as closed*/
            md.dir[fn].perms = 0;
        }

        for (bl = 0; bl < TOTALBLOCKS; bl++){ /*Make every block blank*/
            md.fat[bl].data = FREE_BLOCK;
        }

        eeprom_update_block( (const void *) &md, (void *) EEPROM_START, sizeof(md)); /*writes new file system*/
        printf("File system metadata created\n");
    }
}

void open_for_write(filename fn){
    block start = md.dir[fn].startblock;
    printf("Opening file %u for write\n", fn);
    if(start == FREE_BLOCK){ /*File doesnt exist*/
        md.dir[fn].length = 0;
        md.dir[fn].startblock = getfreeblock();
        if(md.dir[fn].startblock == FREE_BLOCK){
            printf("No memory left in EEPROM\n");
            return;
        } else{
            md.dir[fn].currpos = 0;
            md.dir[fn].perms = 1;
        }
        printf("New file created\n");
    }
    else { /*File Does exist*/
        if(md.dir[fn].currpos != CLOSED_FILE){
            printf("Error whilst opening file for write. File %u already open\n", fn);
            return;
        }

        printf("Overwritting exisiting file %u\n", fn);
        freeBlockChain(start, md.dir[fn].length);

        md.dir[fn].length = 0;
        md.dir[fn].currpos = 0;
        md.dir[fn].perms = 1;
    }
}


void open_for_append(filename fn) {
    block start = md.dir[fn].startblock;
    printf("Opening file %u for appending\n", fn);

    if(start == FREE_BLOCK){ /*File doesnt exist*/
        md.dir[fn].length = 0;
        md.dir[fn].startblock = getfreeblock();
        if(md.dir[fn].startblock == FREE_BLOCK){
            printf("No memory left in EEPROM\n");
            return;
        } else{
            md.dir[fn].currpos = 0;
            md.dir[fn].perms = 1;
        }
        printf("New file created\n");
    }
    else { /*file does exist*/
        if(md.dir[fn].currpos != CLOSED_FILE){
            printf("Error whilst opening file. File already open.");
            return;
        }
        md.dir[fn].perms = 3;
        block idx;
        for(idx=0; idx < md.dir[fn].length; idx++){
            start = md.fat[start].nextLocation;
        }
        md.dir[fn].currpos = start;
    }
}

void close(filename fn){
    block start = md.dir[fn].startblock;
    printf("Closing file %u\n", fn);
    if(start == FREE_BLOCK){
        printf("Error file doesn't exist\n");
        return;
    }

    if(md.dir[fn].currpos == CLOSED_FILE){
        printf("Error file not open\n");
        return;
    }
    md.dir[fn].currpos = CLOSED_FILE;
    md.dir[fn].perms = 0;
    eeprom_update_block( (const void *) &md, (void *) EEPROM_START, sizeof(md)); /*Saves file to EEPROM*/
    printf("File closed\n");
}

void delete(filename fn){
    block start = md.dir[fn].startblock;
    printf("Deleting file %u\n", fn);
    if(start == FREE_BLOCK){
        printf("Error file doesnt exist\n");
        return;
    }

    freeBlockChain(start, md.dir[fn].length);

    md.dir[fn].currpos = CLOSED_FILE;
    md.dir[fn].length = 0;
    md.dir[fn].startblock = FREE_BLOCK;
    md.dir[fn].perms = 0;

    printf("File deleted\n");
    eeprom_update_block( (const void *) &md, (void *) EEPROM_START, sizeof(md));
}

void write(filename fn, uint8_t *buff, uint16_t len) {
    uint16_t idx;
    block currbl = 0;
    block nextbl;
    uint8_t perms = md.dir[fn].perms;


    printf("File %u is writing\n", fn);


    if (md.dir[fn].currpos == CLOSED_FILE ) {
        printf( "Error while writing to file. File not open\n", fn);
        return;
    }
    if (perms == 0 || perms == 2){
        printf("Error while writing to file. File not open for writing\n", fn);
        return;
    }
    else if(perms == 1){
        currbl = md.dir[fn].startblock;
        md.dir[fn].length = len;
    }
    else if(perms == 3){
        currbl = md.dir[fn].currpos;
        md.dir[fn].length += len;
    }
    for (idx=0; idx < len; idx++) {
        printf("Writing data %u to block %u\n", buff[idx], currbl);
        md.fat[currbl].data = buff[idx];
        nextbl = getfreeblock();
        md.fat[currbl].nextLocation = nextbl;
        currbl = nextbl;
    }



}


void open_for_read(filename fn){
    block start = md.dir[fn].startblock;
    printf("Opening file %u for read\n", fn);

    if(start == FREE_BLOCK){
        printf("Error whilst opening file. File does not exist.\n");
        return;
    }
    if(md.dir[fn].currpos != CLOSED_FILE){
        printf("Error whilst opening file. File already open\n");
        return;
    }

    md.dir[fn].currpos = 0;
    md.dir[fn].perms = 2;
    printf("File Opened\n");
}

void read(filename fn){
    block start = md.dir[fn].startblock;
    printf("Reading file %u\n",fn);

    if(md.dir[fn].currpos == CLOSED_FILE){
        printf("Error whilst reading file. File not open\n");
        return;
    }
    if(md.dir[fn].perms != 2){
        printf("Error whilst reading file. Incorrect Permissions (File not opened for reading)\n");
        return;
    }

    block idx;
    char c;
    for(idx=0; idx < md.dir[fn].length; idx++){
        c = md.fat[start].data;
        printf("%c", c);
        start = md.fat[start].nextLocation;
    }
    printf("\n");
}


block getfreeblock(void) { /*Finds first free block available */
    block bl;

    for(bl=0; bl< TOTALBLOCKS; bl++){
        if(md.fat[bl].data == FREE_BLOCK){ /*First free space it finds*/
            md.fat[bl].data = START_BLOCK;
            return bl;
        }
    }
    /*No Space left*/
    return FREE_BLOCK;
}

void freeBlockChain(block start, uint16_t len){ /*Frees a chain of blocks, aka deletes data*/
    uint16_t idx ;

    if(md.fat[start].data == FREE_BLOCK) { /*Block is already free*/
        printf("Error when freeing block chain. Starting block already free\n");
        return;
    }

    idx = 0;
    while (start != START_BLOCK && idx < len){
        md.fat[start].data = FREE_BLOCK;
        printf("Current idx = %u block = %u LENGTH IS %u\n", idx, start,len);
        start = md.fat[start].nextLocation;
        if (start == FREE_BLOCK) { /* Next block is already free */
            printf("Error when freeing block chain. Block %u already free\n", start);
            return;
        }
        idx++;

    }
    md.fat[start].data = FREE_BLOCK;
}







