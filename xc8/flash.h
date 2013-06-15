/* 
 * File:   flash.h
 * Author: srodgers
 *
 * Created on March 22, 2013, 5:57 PM
 */

#ifndef FLASH_H
#define	FLASH_H


#define ROW_WORDS _FLASH_ERASE_SIZE
#define ROW_BYTES (ROW_WORDS << 1)
#define FWRITE_LATCHES 8
#define FWRITE_LOOP_COUNT (ROW_WORDS/FWRITE_LATCHES)

#ifdef	__cplusplus
extern "C" {
#endif

void flash_read_row(uint16_t pm_word_addr, void *buffer);
void flash_write_row(uint16_t pm_word_addr, void *buffer);


#ifdef	__cplusplus
}
#endif

#endif	/* FLASH_H */

