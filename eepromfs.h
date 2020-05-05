/* COMP2215  Task 4*/

#ifndef _EEPROMFS_H_
#define _EEPROMFS_H_

/* Serves as magic cookie to recognize file system:  */
#define FSVERSION_UINT16  18

#define MAX_FILES 10 /*10*/
#define EEPROM_LEN 1600 /*1600*/
#define EEPROM_START 457 /*447 moved by 10 because of perms*/
#define BLOCK_SIZE  64  /*64*/

#define START_BLOCK   (UINT8_MAX - 1U)
#define FREE_BLOCK  UINT8_MAX
#define TOTALBLOCKS  EEPROM_LEN/BLOCK_SIZE

/* If file is unopened, current position is set to: */
#define CLOSED_FILE   UINT16_MAX

void init_eepromfs(void);
void init(void);

void open_for_write(uint8_t filename);
void open_for_append(uint8_t filename);
void open_for_read(uint8_t filename);

void close(uint8_t filename);
void write(uint8_t filename, uint8_t *buff, uint16_t len);
void read(uint8_t filename);
void delete(uint8_t filename);

uint8_t getfreeblock(void);
void freeBlockChain(uint8_t start, uint16_t len);


#endif /* _EEPROMFS_H_ */
