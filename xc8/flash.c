/*
 *
 * Read/write a row of program memory
 *
 * The size of the row is dependent on which microcontroller chip is used,
 * and controlled by the constant ROW_BYTES
 *
 */



#include <xc.h>
#include <stdint.h>
#include "defs.h"
#include "flash.h"

/*
 *
 * Read a row from program memory
 *
 */



void flash_read_row(uint16_t pm_word_addr, void *buffer)
{
    uint16_t *pma = (uint16_t *) buffer;
    uint16_t wa = pm_word_addr & ~(ROW_WORDS - 1); //force row boundary
    uint16_t i;
    EECON1bits.CFGS = FALSE;
    EECON1bits.EEPGD = TRUE;
    for(i = 0; i != ROW_WORDS; i++){
        EEADRL = (uint8_t) wa;
        EEADRH = (uint8_t) (wa >> 8);
        EECON1bits.RD = TRUE;
        NOP();
        NOP();
        *pma++ = (EEDATH << 8) | EEDATL;
        wa++;

    }
    EECON1bits.EEPGD = FALSE;

}


/*
 * Write a row to program memory
 * 
 *
 * Must be called with interrupts disabled!
 *
 */

void flash_write_row(uint16_t pm_word_addr, void *buffer)
{
    uint16_t *b = (uint16_t *) buffer;
    uint16_t wa = pm_word_addr & ~(ROW_WORDS - 1); //force row boundary
    uint16_t v;
    uint8_t i,j;

    /* Erase row */
    EEADRL = (uint8_t) wa;
    EEADRH = (uint8_t) (wa >> 8);

    EECON1bits.EEPGD = TRUE;
    EECON1bits.CFGS = FALSE;
    EECON1bits.FREE = TRUE;
    EECON1bits.WREN = TRUE;

    EECON2 = 0x55;
    EECON2 = 0xAA;

    EECON1bits.WR = TRUE;
    NOP();
    NOP();
    EECON1bits.FREE = FALSE;

    // Write loop
    
    for(i = 0; i != FWRITE_LOOP_COUNT; i++){
        for(j = 0; j != FWRITE_LATCHES; j++){
            EECON1bits.LWLO = ((FWRITE_LATCHES - 1) == j) ? FALSE : TRUE;
            v = *b++;
            EEDATL = (uint8_t) v;
            EEDATH = (uint8_t) (v >> 8);
            EECON2 = 0x55;
            EECON2 = 0xAA;
            EECON1bits.WR = TRUE;
            NOP();
            NOP();
            EEADRL++;
        }
    }
    EECON1bits.WREN = FALSE;
}
